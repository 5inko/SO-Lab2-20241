#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

//  Creación de una estructura vector para facilitar el trabajo
#define VECTOR_CAPACITY 8

typedef struct Vector{
    char **data;
    int capacity;
    int size;
} Vector;

Vector vector_create() {
    Vector v;
    v.data = (char**) malloc (VECTOR_CAPACITY * sizeof(char*));
    v.capacity = VECTOR_CAPACITY;
    v.size = 0;
    return v;
}

void vector_add(Vector* v, char *str){
    if (v->capacity == v->size) {
        char** new_data = (char**) malloc( (v->capacity << 1) * sizeof(char*) );
        int n = v->size;
        for (int i = 0; i < n; i++) new_data[i] = v->data[i];
        free(v->data);
        v->data = new_data;
        v-> capacity <<= 1;
    }
    v->data[v->size++] = str;
}

char *vector_get(Vector* v, int index){
    if (index < 0 || index >= v->size) return NULL;
    return v->data[index];
}

int vector_get_index(Vector *v, char *val){
    int n = v->size;
    for(int i = 0; i < n; i++){
        if (strcmp(val, v-> data[i]) == 0) return i;
    }
    return -1;
}

void vector_destroy(Vector *v){
    free(v->data);
}

Vector PATH;

// Utility functions
void print_error(){
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message)); 
}

int is_delimiter(char c){
    if (c == ' ' ||  c == '\t' || c == '\n' || c == '>' || c == '&'){
        return 1;
    }
    return 0;
}

Vector parse_input(char *expression){
    Vector items = vector_create();

    int n = strlen(expression);
    char* s = NULL;
    int start = -1;
    for (int i = 0; i < n; i++) {
        if (expression[i] == '>') {
            vector_add(&items, ">");
            continue;
        }
        if (expression[i] == '&') {
            vector_add(&items, "&");
            continue;
        }
        if (!is_delimiter(expression[i])) {
            if (i == 0 || is_delimiter(expression[i-1])) start = i;
            if (i == n-1 || is_delimiter(expression[i+1])) {
                s = strndup(expression+start, i-start+1);
                vector_add(&items, s);
            }
        }
    }

    return items;
}

// Handle commands functions
int handle_builtin_commands(Vector items){
    if (strcmp(vector_get(&items, 0), "exit") == 0){
        if(items.size > 1){
            print_error();            
        } else {
            exit(0);
        }
        return 1;
    } else if (strcmp(vector_get(&items, 0), "cd") == 0) {
        if(chdir(vector_get(&items, 1)) != 0){
            // Test 1 and 2: Input to check bad cd. No arguments or 2 arguments are passed to cd.
            print_error();
        }
        return 1;
    } else if (strcmp(vector_get(&items, 0), "path") == 0){
        Vector new_path = vector_create();

        for (int i = 1; i < items.size; i++){
            char* aux = (char*) malloc((strlen(vector_get(&items, i)) + strlen("/") + 1) * sizeof(char));
            strcpy(aux, vector_get(&items, i));
            strcat(aux, "/");

            // Agregar el nuevo string a local_path
            vector_add(&new_path, aux);
        }

        PATH = new_path;
        return 1;
    }
    return 0;
}

int handle_external_commands(Vector items){
    char *command = vector_get(&items, 0);
    int pos = vector_get_index(&items, ">");
    if (pos == -1) {
        pos = items.size;
    }
    for (int i = 0; i < PATH.size; i++) {
        char *dir = (char*) malloc((strlen(vector_get(&PATH, i)) + strlen(command) + 1) * sizeof(char));
        snprintf(dir, strlen(vector_get(&PATH, i)) + strlen(command) + 1, "%s%s", vector_get(&PATH, i), command);
        
        if (access(dir, X_OK) == 0) {
            char* argv[pos+1];
            for (int i = 0; i < pos; i++){
                argv[i] = vector_get(&items, i);
            } 
            argv[pos] = NULL;
            
            int rc = fork();
            if (rc == 0) {
                if (pos != items.size) {
                    close(STDOUT_FILENO);
                    open(vector_get(&items, items.size - 1), O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
                }
                execv(dir, argv);
            }
            return rc;
        }
    }

    print_error();
    return -1;   
}

int is_valid_redirection (Vector items) {
    int n = items.size;
    for (int i = 0; i < n; i++) {
        if (strcmp(">", vector_get(&items, i)) == 0) {
            if (i == 0 || i == n-1 || n-1-i > 1){
                return 0;
            } 
            if (i != n-1 && strcmp(">", vector_get(&items, i+1)) == 0){
                return 0;
            } 
        }
    }
    return 1;
}

int main(int argc, char *argv[]){
    int in_exec, full_cmd;
    Vector items, actual;
    char *expression = NULL;

    FILE *input_stream = NULL;

    for (int i = 1; i < argc; i++) {
        FILE* aux_file = fopen(argv[i], "r");
        if (aux_file == NULL) {
            print_error();
            exit(1);
        }

        if (input_stream == NULL) {
            input_stream = aux_file;
        } else {
            struct stat in_stat, aux_stat;
            fstat(fileno(input_stream), &in_stat);
            fstat(fileno(aux_file), &aux_stat);
            if (!(in_stat.st_dev == aux_stat.st_dev && in_stat.st_ino == aux_stat.st_ino)) {
                print_error();
                exit(1);
            }
        }
    }

    if(input_stream == NULL){
        input_stream = stdin;
    }

    vector_add(&PATH, "./");
    vector_add(&PATH, "/usr/bin/");
    vector_add(&PATH, "/bin/");

    while(1){
        // Verificar si estamos leyendo desde un archivo
        if (input_stream == stdin) {
            printf("wish> ");
        }

        // Leer la entrada
        size_t n = 0;
        int in_len = getline(&expression, &n, input_stream);

        if(in_len == -1){
            if (input_stream == stdin){
                continue;
            } else {
                break;
            }
        }

        // Verificar si se alcanzó el final del archivo (EOF)
        if (feof(input_stream)) {
            break;
        }

        // Cambiar el último caracter por NULL
        expression[in_len-1] = '\0';

        // Separar la entrada en items
        items = parse_input(expression);

        if(vector_get(&items, 0) == NULL){
            continue;
        }
 
        actual = vector_create();

        int pids[items.size];
        int pid_count = 0;
        for(int i = 0; i < items.size; i++){
            full_cmd = 0;
            if(strcmp(vector_get(&items, i), "&") == 0){
                continue;
            }else{
                char *pos_act;
                while(!full_cmd && i < items.size){
                    pos_act = vector_get(&items, i);
                    if (strcmp(pos_act, "&") != 0){
                        //printf("Añadiendo %s a actual\n", pos_act);
                        vector_add(&actual, pos_act);
                        i++;
                    } else {
                        full_cmd = 1;
                    }
                }
                // Verificar si los items tienen una redireccion válida
                if (is_valid_redirection(actual)) {
                    // Intentar ejecutar un comando interno (revisar si es un comando interno)
                    in_exec = handle_builtin_commands(actual);

                        
                    // En caso contrario intentar ejecutar un externo
                    if(in_exec == 0){
                        pids[pid_count] = handle_external_commands(actual);
                        pid_count++;
                    }        
                } else {
                    print_error();
                }
                full_cmd = 0;
                actual = vector_create();
            }
        }
        for(int i = 0; i < pid_count; i++){
            waitpid(pids[i], NULL, 0);
        }

        free(expression);
        vector_destroy(&items);
        vector_destroy(&actual);
    }

    return 0;
}