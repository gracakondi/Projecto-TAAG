#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdint.h>
#include <sys/select.h> // Inclui para usar select()
#include <stdbool.h>

#define INICIAL 10 // nº de Threads/"clientes"
#define TRUE 1

typedef struct {
	int pnr;
	int reserva;
	int consulta;
	int cancelamento;
} buffer;

buffer *regicao_critica;  // Mudança para alocação dinamica

pthread_mutex_t mutex;
sem_t sem_reserva;
sem_t sem_consulta;

void* reserva(void* args);
void* consulta(void* args);
void* cancelamento(void* args);

void ver_dados();
void* Thread(void* args); // thread principal
void tratamento_interrupcao();
void verificar_interrupcao();

int main() {
	pthread_t threads[INICIAL];
	int i;

	srand(time(NULL));

	// Alocacao dinamica de memoria para o buffer
	regicao_critica = (buffer*)malloc(INICIAL * sizeof(buffer));

	if (regicao_critica == NULL) {
		printf("Erro ao alocar memoria para o buffer.\n");
		return 1;
	}

	pthread_mutex_init(&mutex, NULL);
	sem_init(&sem_reserva, 0, 5);  // Limite de 5 threads de reserva ao mesmo tempo
	sem_init(&sem_consulta, 0, 5); // Limite de 5 threads de consulta ao mesmo tempo

	for (i = 0; i < 10; i++) {
		intptr_t buffer_index = i;
		pthread_create(&threads[i], NULL, Thread, (void*)buffer_index);
	}

	for (i = 0; i < INICIAL; i++)
		pthread_join(threads[i], NULL);

	// Liberacao da memoria alocada dinamicamente
	free(regicao_critica);
	pthread_mutex_destroy(&mutex);
	sem_destroy(&sem_reserva);
	sem_destroy(&sem_consulta);

	printf("Finalizado.\n");
	return 0;
}

void* reserva(void* args) {
	sem_wait(&sem_reserva);
	tratamento_interrupcao();  // Simula interrupcao durante a operacao

	int buf_index = (intptr_t)args;
	int pnr = (unsigned int)pthread_self();

	pthread_mutex_lock(&mutex);
	regicao_critica[buf_index].pnr = pnr;
	regicao_critica[buf_index].reserva = 1;
	regicao_critica[buf_index].consulta = 0;
	regicao_critica[buf_index].cancelamento = 0;
	pthread_mutex_unlock(&mutex);

	printf("[Reserva] PNR da thread: %d\n", pnr);
	ver_dados();

	sem_post(&sem_reserva);
	return NULL;
}

void* consulta(void* args) {
	sem_wait(&sem_consulta);
	tratamento_interrupcao();  // Simula interrupcao durante a operacao

	int buf_index = (intptr_t)args;
	int pnr = (unsigned int)pthread_self();

	pthread_mutex_lock(&mutex);
	regicao_critica[buf_index].pnr = pnr;
	regicao_critica[buf_index].reserva = 0;
	regicao_critica[buf_index].consulta = 1;
	regicao_critica[buf_index].cancelamento = 0;
	pthread_mutex_unlock(&mutex);

	printf("[Consulta] PNR da thread: %d\n", pnr);
	ver_dados();

	sem_post(&sem_consulta);
	return NULL;
}

void* cancelamento(void* args) {
	sem_wait(&sem_consulta);
	tratamento_interrupcao();  // Simula interrupcao durante a operacao

	int buf_index = (intptr_t)args;
	int pnr = (unsigned int)pthread_self();

	pthread_mutex_lock(&mutex);
	regicao_critica[buf_index].pnr = pnr;
	regicao_critica[buf_index].reserva = 0;
	regicao_critica[buf_index].consulta = 0;
	regicao_critica[buf_index].cancelamento = 1;
	pthread_mutex_unlock(&mutex);

	printf("[Cancelamento] PNR da thread: %d\n", pnr);
	ver_dados();

	sem_post(&sem_consulta);
	return NULL;
}

void ver_dados() {
	int i; 
	pthread_mutex_lock(&mutex);
	printf("\n=== Processos/Threads em execucao ===\n");

	for (i = 0; i < INICIAL; i++)
		printf("PNR: %d | Reserva: %d | Consulta: %d | Cancelamento: %d\n", regicao_critica[i].pnr, regicao_critica[i].reserva, regicao_critica[i].consulta, regicao_critica[i].cancelamento);
	
	pthread_mutex_unlock(&mutex);
}

void* Thread(void* args) {
	int index = (intptr_t)args;
	int i; 
	
	while (true) {
		verificar_interrupcao();
		int f = rand() % 3;
		pthread_t thread;
		if (f == 0)
			pthread_create(&thread, NULL, reserva, (void*)(intptr_t)index);
		else if (f == 1)
			pthread_create(&thread, NULL, consulta, (void*)(intptr_t)index);
		else
			pthread_create(&thread, NULL, cancelamento, (void*)(intptr_t)index);

		pthread_join(thread, NULL);
	}

	return NULL;
}

// Funcao para simular a interrupcao durante o processo
void tratamento_interrupcao() {
  sleep(1);  // Simula uma "interrupcao" do processo, com uma pausa de 1 segundo
}

// Verifica se a barra de espaço foi pressionada (com select())
void verificar_interrupcao() {
    char c;
    bool pausa_global;
    
    
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    tv.tv_sec = 0; // Verifica imediatamente (sem timeout)
    tv.tv_usec = 0;

    int retval = select(1, &rfds, NULL, NULL, &tv);

    if (retval == -1) {
        perror("select()");
        exit(1);
    } else if (retval) {
        if (read(STDIN_FILENO, &c, 1) > 0 && c == ' ') {
            printf("\n[Interrupcao] Barra de espaço pressionada. Pausando por 5 segundos...\n");
            pausa_global = true; // Ativa a flag de pausa
            sleep(5);
            pausa_global = false; // Desativa a flag de pausa
        }
    }
}
