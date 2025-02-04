#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdint.h>

#define INICIAL 10

typedef struct {
    int pnr;
    int reserva;
    int consulta;
    int cancelamento;
} buffer;

buffer *buff;  // Mudança para alocação dinâmica

pthread_mutex_t mutex;
sem_t sem_reserva;
sem_t sem_consulta;

void* reserva(void* args);
void* consulta(void* args);
void ver_dados();
void* Thread(void* args);
void tratamento_interrupcao();

int main() {
    pthread_t threads[INICIAL];
    int i;

    srand(time(NULL));

    // Alocação dinâmica de memória para o buffer
    buff = (buffer*)malloc(INICIAL * sizeof(buffer));
    if (buff == NULL) {
        printf("Erro ao alocar memória para o buffer.\n");
        return 1;
    }

    pthread_mutex_init(&mutex, NULL);
    sem_init(&sem_reserva, 0, 5);  // Limite de 5 threads de reserva ao mesmo tempo
    sem_init(&sem_consulta, 0, 5); // Limite de 5 threads de consulta ao mesmo tempo

    for (i = 0; i < 10; i++) {
        intptr_t buffer_index = i;
        pthread_create(&threads[i], NULL, Thread, (void*)buffer_index);
    }

    for (i = 0; i < INICIAL; i++) {
        pthread_join(threads[i], NULL);
    }

    // Liberação da memória alocada dinamicamente
    free(buff);
    pthread_mutex_destroy(&mutex);
    sem_destroy(&sem_reserva);
    sem_destroy(&sem_consulta);

    printf("Finalizado.\n");
    return 0;
}

void* reserva(void* args) {
    sem_wait(&sem_reserva);
    tratamento_interrupcao();  // Simula interrupção durante a operação

    int buf_index = (intptr_t)args;
    int pnr = (unsigned int)pthread_self();

    pthread_mutex_lock(&mutex);
    buff[buf_index].pnr = pnr;
    buff[buf_index].reserva = 1;
    buff[buf_index].consulta = 0;
    buff[buf_index].cancelamento = 0;
    pthread_mutex_unlock(&mutex);

    printf("[Reserva] PNR da thread: %d\n", pnr);
    ver_dados();

    sem_post(&sem_reserva);
    return NULL;
}

void* consulta(void* args) {
    sem_wait(&sem_consulta);
    tratamento_interrupcao();  // Simula interrupção durante a operação

    int buf_index = (intptr_t)args;
    int pnr = (unsigned int)pthread_self();

    pthread_mutex_lock(&mutex);
    buff[buf_index].pnr = pnr;
    buff[buf_index].reserva = 0;
    buff[buf_index].consulta = 1;
    buff[buf_index].cancelamento = 0;
    pthread_mutex_unlock(&mutex);

    printf("[Consulta] PNR da thread: %d\n", pnr);
    ver_dados();

    sem_post(&sem_consulta);
    return NULL;
}

void ver_dados() {
    int i; 
    pthread_mutex_lock(&mutex);
    printf("\n=== DADOS ATUAIS ===\n");
    for (i = 0; i < INICIAL; i++) {
        printf("PNR: %d | Reserva: %d | Consulta: %d | Cancelamento: %d\n",
               buff[i].pnr, buff[i].reserva, buff[i].consulta, buff[i].cancelamento);
    }
    pthread_mutex_unlock(&mutex);
}

void* Thread(void* args) {
    int index = (intptr_t)args;
    int i; 
    for (i = 0; i < 5; i++) {
        int f = rand() % 2;
        pthread_t thread;

        if (f == 0) {
            pthread_create(&thread, NULL, reserva, (void*)(intptr_t)index);
        } else {
            pthread_create(&thread, NULL, consulta, (void*)(intptr_t)index);
        }

        pthread_join(thread, NULL);
    }

    return NULL;
}

// Função para simular a interrupção durante o processo
void tratamento_interrupcao() {
    sleep(1);  // Simula uma "interrupção" do processo, com uma pausa de 1 segundo
}
