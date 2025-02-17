#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>

// Estrutura para armazenar cada reserva (PNR)
typedef struct PNRNode {
    int pnr;
    time_t timestamp;         // Horário da reserva
    int pago;                 //0 se nao pago, 1 se pago
    struct PNRNode *next;
} PNRNode;

PNRNode *meuPNR = NULL;         // Lista encadeada de reservas

// Mutex para proteger o acesso à lista de reservas
pthread_mutex_t meuPNR_mutex;

// Semáforos para controle das operações
sem_t sem_reserva, sem_consulta, sem_pagamento, sem_cancelamento;

// Função auxiliar para gerar um PNR aleatório (entre 1000 e 9999)
int gerarPNR() {
    return 1000 + rand() % 9000;
}

// Função que insere uma nova reserva na lista
void adicionarReserva() {
    PNRNode *novoNo = (PNRNode*)malloc(sizeof(PNRNode));
    if (!novoNo) {
        perror("Erro ao alocar memória");
        return;
    }
    novoNo->pnr = gerarPNR();
    novoNo->timestamp = time(NULL);
    novoNo->next = meuPNR;
    meuPNR = novoNo;
    printf("Reserva realizada: %d\n", novoNo->pnr);
}

// Função que remove um nó aleatório da lista e retorna o PNR removido.
// Retorna 1 se removeu uma reserva ou 0 se a lista estava vazia.
int removerReservaAleatoria(int *pnrRemovido) {
    if (meuPNR == NULL)
        return 0;
    int count = 0;
    PNRNode *temp = meuPNR;
    while (temp) {
        count++;
        temp = temp->next;
    }
    int index = rand() % count; // índice aleatório
    PNRNode *atual = meuPNR;
    PNRNode *anterior = NULL;
    for (int i = 0; i < index; i++) {
        anterior = atual;
        atual = atual->next;
    }
    if (anterior == NULL)
        meuPNR = atual->next;
    else
        anterior->next = atual->next;
    *pnrRemovido = atual->pnr;
    free(atual);
    return 1;
}

// Função que seleciona um nó aleatório (sem removê-lo) e retorna seu PNR.
int obterReservaAleatoria(int *pnr) {
    if (meuPNR == NULL)
        return 0;
    int count = 0;
    PNRNode *temp = meuPNR;
    while (temp) {
        count++;
        temp = temp->next;
    }
    int index = rand() % count;
    temp = meuPNR;
    for (int i = 0; i < index; i++) {
        temp = temp->next;
    }
    *pnr = temp->pnr;
    return 1;
}

// Função para exibir todas as reservas atuais
void imprimirReservas() {
    pthread_mutex_lock(&meuPNR_mutex);
    printf("\n=== Reservas Atuais ===\n");
    PNRNode *temp = meuPNR;
    while (temp) {
        printf("PNR: %d\n", temp->pnr);
        temp = temp->next;
    }
    pthread_mutex_unlock(&meuPNR_mutex);
}

// Thread que exibe o conteúdo da variável meuPNR a cada 30 segundos
void* impressao_thread(void* arg) {
    while (1) {
        sleep(30); // Alterado para 30 segundos
        pthread_mutex_lock(&meuPNR_mutex);
        printf("\n\n=== Reservas Atuais (a cada 30 segundos) ===\n\n");
        PNRNode *temp = meuPNR;
        while (temp) {
            printf("PNR: %d\n", temp->pnr);
            temp = temp->next;
        }
        pthread_mutex_unlock(&meuPNR_mutex);
    }
    return NULL;
}

// Função de reserva (adiciona um novo PNR)
void* reserva_func(void* arg) {
    sem_wait(&sem_reserva);
    pthread_mutex_lock(&meuPNR_mutex);
    adicionarReserva();
    pthread_mutex_unlock(&meuPNR_mutex);
    sem_post(&sem_reserva);
    return NULL;
}

// Função de cancelamento (remove um PNR aleatório)
void* cancelamento_func(void* arg) {
    sem_wait(&sem_cancelamento);
    pthread_mutex_lock(&meuPNR_mutex);
    int pnrRemovido;
    if (removerReservaAleatoria(&pnrRemovido))
        printf("Reserva cancelada: %d\n", pnrRemovido);
    else
        printf("Nenhuma reserva para cancelar.\n");
    pthread_mutex_unlock(&meuPNR_mutex);
    sem_post(&sem_cancelamento);
    return NULL;
}

// Função de consulta (seleciona um PNR aleatório sem removê-lo)
void* consulta_func(void* arg) {
    sem_wait(&sem_consulta);
    pthread_mutex_lock(&meuPNR_mutex);
    int pnr;
    if (obterReservaAleatoria(&pnr))
        printf("Consulta feita com sucesso: %d\n", pnr);
    else
        printf("Nenhuma reserva para consultar.\n");
    pthread_mutex_unlock(&meuPNR_mutex);
    sem_post(&sem_consulta);
    return NULL;
}

// Função de pagamento (marca um PNR como pago)
void* pagamento_func(void* arg) {
    sem_wait(&sem_pagamento);
    pthread_mutex_lock(&meuPNR_mutex);
    
    // Verifica se há reservas na lista
    if (meuPNR == NULL) {
        printf("Não há reservas para pagamento.\n");
    } else {
        PNRNode *temp = meuPNR;
        
        // Percorre a lista procurando um PNR não pago
        while (temp) {
            if (temp->pago == 0) {  // Se o PNR não foi pago
                temp->pago = 1;  // Marca como pago
                printf("Pagamento feito com sucesso: PNR %d\n", temp->pnr);
                break;  // Para de procurar assim que encontrar o primeiro não pago
            }
            temp = temp->next;
        }
        
        if (temp == NULL) {
            printf("Todos os PNRs já foram pagos.\n");
        }
    }

    pthread_mutex_unlock(&meuPNR_mutex);
    sem_post(&sem_pagamento);
    return NULL;
}

// Thread que verifica a cada 5 segundos se algum PNR está pendente há 60 segundos.
// Se estiver, remove-o e exibe a mensagem correspondente.
void* verificador_timeout(void* arg) {
    while (1) {
        sleep(5);  // Aguarda 5 segundos antes de verificar novamente
        pthread_mutex_lock(&meuPNR_mutex);
        
        time_t agora = time(NULL);
        PNRNode *atual = meuPNR;
        PNRNode *anterior = NULL;

        while (atual) {
            double diff = difftime(agora, atual->timestamp);
            if (diff >= 60 && atual->pago == 0) {  // Se o PNR não foi pago após 1 minuto
                int pnrComTimeout = atual->pnr;
                
                // Remove a reserva não paga da lista
                if (anterior == NULL) {
                    meuPNR = atual->next;  // Se for o primeiro elemento, ajusta o ponteiro da cabeça
                    free(atual);
                    atual = meuPNR;  // Avança para o próximo elemento
                } else {
                    anterior->next = atual->next;  // Remove o PNR da lista
                    free(atual);
                    atual = anterior->next;  // Avança para o próximo elemento
                }

                printf("PNR: %d não foi pago e foi removido.\n", pnrComTimeout);  // Exibe a reserva removida
            } else {
                anterior = atual;
                atual = atual->next;
            }
        }
        
        pthread_mutex_unlock(&meuPNR_mutex);
    }
    return NULL;
}


int main() {
    srand(time(NULL));
    pthread_mutex_init(&meuPNR_mutex, NULL);
    sem_init(&sem_reserva, 0, 1);
    sem_init(&sem_consulta, 0, 1);
    sem_init(&sem_pagamento, 0, 1);
    sem_init(&sem_cancelamento, 0, 1);

    // Cria a thread que verifica os PNRs com timeout
    pthread_t timeoutThread;
    pthread_create(&timeoutThread, NULL, verificador_timeout, NULL);

    // Cria a thread que exibe as reservas a cada 30 segundos
    pthread_t printThread;
    pthread_create(&printThread, NULL, impressao_thread, NULL);

    // As primeiras 8 operações serão de reserva
    pthread_t threads[20];
    for (int i = 0; i < 20; i++) {
        pthread_create(&threads[i], NULL, reserva_func, NULL);
        pthread_join(threads[i], NULL);
    }

    //criando threads para pagamentos
    pthread_t threads_pagamentos[10];
    for(int i = 0; i < 10; i++)
    {
       pthread_create(&threads_pagamentos[i], NULL, pagamento_func, NULL);
        pthread_join(threads_pagamentos[i], NULL);
    }
    
    // Exibe inicialmente o conteúdo da variável meuPNR
    imprimirReservas();
    // Loop alternado entre reserva, pagamento, consulta e cancelamento
    while (1) {
        int op = rand() % 4; // 0: reserva, 1: pagamento, 2: consulta, 3: cancelamento
        pthread_t opThread;
        switch(op) {
            case 0:
                for(int i = 0; i <= 3; i++)
                   pthread_create(&opThread, NULL, reserva_func, NULL);
                break;
            case 1:
                   pthread_create(&opThread, NULL, pagamento_func, NULL);
                break;
            case 2:
                pthread_create(&opThread, NULL, consulta_func, NULL);
                break;
            case 3:
                pthread_create(&opThread, NULL, cancelamento_func, NULL);
                break;
        }
        pthread_join(opThread, NULL);

        // Intervalo entre operações
        sleep(1);
    }

    // Código de limpeza (nunca alcançado neste exemplo)
    pthread_mutex_destroy(&meuPNR_mutex);
    sem_destroy(&sem_reserva);
    sem_destroy(&sem_consulta);
    sem_destroy(&sem_pagamento);
    sem_destroy(&sem_cancelamento);

    return 0;
}

