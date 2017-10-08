#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/wait.h>

#define CICLOS 10
#define MAXQUEUE 10

typedef struct queue_s { // Estructura de cola
  int head;
  int tail;
  int elements;
  int array[MAXQUEUE];
} queue_t;

typedef struct semaphore_s { // Estructura de semáforo
  int counter;
  queue_t queue;
} sem_t;

char *pais[3]={"Peru","Bolivia","Colombia"};
int *g; // Bandera de sincronizacion
int *h; // Bandera de sincronizacion
sem_t *sem; // Semáforo

// Macro que incluye el código de la instrucción máquina xchg
#define atomic_xchg(A,B)  __asm__ __volatile__( \
  " lock xchg %1,%0 ;\n"  \
  : "=ir" (A)             \
  : "m" (B), "ir" (A)      \
);

void initqueue(queue_t * queue) // Inicializa una cola
{
  queue->head = 0;
  queue->tail = 0;
  queue->elements = 0;
}

void push(queue_t * queue, int value) // Se guarda un valor en la cola
{
	queue->array[queue->tail]=value;
	queue->tail++;
	queue->tail = queue->tail%MAXQUEUE;
  queue->elements++;
}

int pop(queue_t * queue) // Se quita un valor de la cola
{
	int valret;
	valret = queue->array[queue->head];
	queue->head++;
	queue->head = queue->head%MAXQUEUE;
  queue->elements--;
	return(valret);
}

int isempty(queue_t * queue) // Devuelve 1 si la cola está vacía
{
  return(queue->elements==0);
}

void initsem(sem_t * sem, int value) // Inicializa un semáforo
{
  sem->counter = value;
  initqueue(&sem->queue);
}

void waitsem(sem_t * sem) // Función de espera de semáforo
{
  int l = 1;
  do { atomic_xchg(l,*g); } while(l!=0); // Puede pasar si otro proceso no esta en esta función
  if(sem->counter > 0) // Si hay permiso para pasar, continúa
  {
    sem->counter--;
  }
  else // Si no hay permiso de paso, se bloquea el proceso
  {
    sem->counter--;
    push(&sem->queue, getpid());;
    kill(getpid(), SIGSTOP);
  }
  *g=0; // Otro proceso puede entrar a la sección atómica
}

void signalsem(sem_t * sem)
{
  int l = 1;
  do { atomic_xchg(l,*h); } while(l!=0); // Puede pasar si otro proceso no esta en esta función
  if(isempty(&sem->queue)) // Si no hay procesos en espera, se suma al contador
  {
    sem->counter++;
  }
  else if(sem->counter <= 0) // Se desbloquea el proximo proceso en la cola
  {
    int p_id;
    sem->counter++;
    p_id = pop(&sem->queue);
    kill(p_id, SIGCONT);
  }
  *h=0;
}

void proceso(int i)
{
  int k;
  for(k = 0; k<CICLOS ; k++)
  {
    waitsem(sem);
    printf("+ Entra %s ",pais[i]);
    fflush(stdout);
    sleep(rand()%3);
    printf("- %s Sale\n",pais[i]);
    signalsem(sem);
    sleep(rand()%3);   // Espera aleatoria fuera de la sección crítica
  }
  exit(0); // Termina el proceso
}

int main()
{
  int pid;
  int status;
  int shmid_sem, shmid_g, shmid_h;
  int args[3];
  int i;
  void *thread_result;

  // Solicitar memoria compartida
  shmid_sem = shmget(0x1234,sizeof(sem),0666|IPC_CREAT);
  shmid_g = shmget(0x2345,sizeof(g),0666|IPC_CREAT);
  shmid_h = shmget(0x2346,sizeof(h),0666|IPC_CREAT);
  if(shmid_sem==-1 || shmid_g ==-1 || shmid_h ==-1 )
  {
    perror("Error en la memoria compartida\n");
    exit(1);
  }
  // Conectar las variables a la memoria compartida
  sem = shmat(shmid_sem,NULL,0);
  g = shmat(shmid_g,NULL,0);
  h = shmat(shmid_h,NULL,0);
  if(sem == NULL)
  {
    perror("Error en el shmat\n");
    exit(2);
  }

  initsem(sem, 1);
  *g = 0;
  *h = 0;

  srand(getpid());
  for(i=0;i<3;i++)
  {
    // Crea un nuevo proceso hijo que ejecuta la función proceso()
    pid = fork();
    if(pid == 0)
    {
      proceso(i);
    }
  }

  for(i=0;i<3;i++)
  {
    pid = wait(&status);
  }

  // Eliminar la memoria compartida
  shmdt(sem);
  shmdt(g);
  shmdt(h);
}
