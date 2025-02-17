#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>

#define INICIAL 10 // n de Threads/"clientes"

typedef struct {
    int pnr;
    int reserva;
    int consulta;
    int cancelamento;
} buffer;

buffer *buff;  // Mudanca para alocacao dinamica

pthread_mutex_t mutex;
sem_t sem_reserva;
sem_t sem_consulta;

void* reserva(void* args);
void* consulta(void* args);
void ver_dados();
void* Thread(void* args);
void tratamento_interrupcao();
void verificar_interrupcao();

volatile int exibir_dados_periodicamente = 1;  // Flag para controle

// Configura o terminal para entrada n bloqueante
void configurar_terminal(int enable) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);

    if (enable) {
        t.c_lflag &= ~(ICANON | ECHO); // Desativa buffer de linha e eco
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
    } else {
        t.c_lflag |= (ICANON | ECHO); // Restaura configuração padrão
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

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

    configurar_terminal(1);  // Configurar terminal para leitura não bloqueante

    for (i = 0; i < INICIAL; i++) {
        intptr_t buffer_index = i;
        pthread_create(&threads[i], NULL, Thread, (void*)buffer_index);
    }

    for (i = 0; i < INICIAL; i++) {
        pthread_join(threads[i], NULL);
    }

    configurar_terminal(0);  // Restaurar terminal

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
        verificar_interrupcao();
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

// Verifica se a barra de espaço foi pressionada
void verificar_interrupcao() {
    char c;
    if (read(STDIN_FILENO, &c, 1) > 0 && c == ' ') {
        printf("\n[Interrupção] Barra de espaço pressionada. Pausando por 5 segundos...\n");
        sleep(5);
    }
}
