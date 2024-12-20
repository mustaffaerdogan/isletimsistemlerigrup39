#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// 1) Prompt: Kullanıcıdan komut girişi bekleyen işlev.
void prompt() {
    printf("> ");
    fflush(stdout);
}

// 2) Quit: Programdan çıkış yaparken arka plan işlemlerini bekleyen işlev.
// Terminale quit girildiğinde çıkılır
void quit(int background_pid) {
    if (background_pid != -1) {
        waitpid(background_pid, NULL, 0);  // Arka plan işlemlerini bekle
    }
    exit(0);  // Programdan çık
}

// 3) Tekli Komutlar: Tek bir komut çalıştıran işlev (örn. ls, pwd).
// Dosya ve dizin listesi almak için: $ ls
// Mevcut çalışma dizinini görmek için: $ pwd
// Bir metni ekrana yazdırmak için: $ echo "Hello, World!" vs vs 
void execute_command(char *command) {
    pid_t pid = fork();

    if (pid == 0) {  // Alt süreç
        char *args[256];
        int i = 0;
        char *token = strtok(command, " \n");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " \n");
        }
        args[i] = NULL;

        execvp(args[0], args);  // Komutu çalıştır
        perror("exec failed");
        exit(1);
    } else if (pid > 0) {  // Ana süreç
        wait(NULL);  // Alt sürecin bitmesini bekle
    } else {
        perror("fork failed");
        exit(1);
    }
}


// Ana program döngüsü: Kullanıcıdan komutları alıp ilgili işlevlere yönlendiren döngü.
int main() {
    char command[256];
    pid_t bg_pid = -1;

    while (1) {
        prompt();

        if (!fgets(command, sizeof(command), stdin)) {
            break;
        }

        if (strncmp(command, "quit", 4) == 0) {
            quit(bg_pid);
        } else if (strchr(command, '&')) {
            background_process(command, &bg_pid);
        } else if (strchr(command, '<')) {
            redirect_input(command);
        } else if (strchr(command, '>')) {
            redirect_output(command);
        } else if (strchr(command, '|')) {
            pipe_commands(command);
        } else {
            execute_command(command);
        }
    }

    return 0;
}

