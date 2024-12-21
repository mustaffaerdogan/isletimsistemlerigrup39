#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <bits/sigaction.h>
#include <asm-generic/signal-defs.h>

// 1) Prompt: Kullanıcıdan komut girişi bekleyen işlev.
void prompt() {
    printf("> ");
    fflush(stdout);
}

// 2) Quit: Programdan çıkış yaparken arka plan işlemlerini bekleyen işlev.
// Terminale quit girildiğinde çıkılır
void quit() {
    pid_t pid;
    int status;

    // Tüm arka plan işlemlerini bekle
    while ((pid = waitpid(-1, &status, 0)) > 0) {
        printf("[PID: %d] retval: %d\n", pid, WEXITSTATUS(status));
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

// 4) Giriş Yönlendirme: Komutun girişini bir dosyadan okuyan işlev.
void redirect_input(char *command) {
    char *cmd = strtok(command, "<");
    char *input_file = strtok(NULL, "\n");

    if (!input_file) {
        printf("Giriş dosyası bulunamadı.\n");
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {
        int fd = open(input_file, O_RDONLY);
        if (fd == -1) {
            printf("Giriş dosyası bulunamadı.\n");
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);

        char *args[256];
        int i = 0;
        char *token = strtok(cmd, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        execvp(args[0], args);
        perror("exec failed");
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("fork failed");
        exit(1);
    }
}
// 5) Çıkış Yönlendirme: Komutun çıktısını bir dosyaya yazdıran işlev.
// $ ls>output.txt => Bu komut, ls komutunun çıktısını output.txt adlı bir dosyaya yazdırır. Mevcut dizindeki tüm dosya ve klasörlerin listesi output.txt'ye kaydedilecektir.
// $ > cat output.txt => output.txt dosyasını kontrol etmek için program içinde cat komutunu kullanabilirsiniz:

void redirect_output(char *command) {
    char *cmd = strtok(command, ">");
    char *output_file = strtok(NULL, "\n");

    if (!output_file) {
        printf("Çıkış dosyası belirtilmedi.\n");
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {
        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror("Dosya açılamadı");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);

        char *args[256];
        int i = 0;
        char *token = strtok(cmd, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        execvp(args[0], args);
        perror("exec failed");
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("fork failed");
        exit(1);
    }
}

// 6) Arka Plan İşlemleri: Komutları arka planda çalıştıran işlev. Bu özellik, bir komutu arka planda çalıştırmak için kullanılır. Arka planda çalışan komutlar, terminaldeki diğer işlemleri engellemeden çalışmaya devam eder. Bu işlev, komutların sonuna & ekleyerek arka planda çalıştırılmasını sağlar.
// SIGCHLD işleyicisi olarak kullanılacak fonksiyon
void child_handler(int signo) {
    int status;
    pid_t completed_pid;

    // Tamamlanan arka plan işlemlerini kontrol et
    while ((completed_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("[PID: %d] retval: %d\n", completed_pid, WEXITSTATUS(status));
        fflush(stdout);
    }
}

void background_process(char *command) {
    // Komutun sonundaki '&' işaretini kaldır
    char *background_flag = strchr(command, '&');
    if (background_flag) {
        *background_flag = '\0';  // '&' işaretini kaldır
        // Komutun sonundaki gereksiz boşlukları temizle
        char *end = background_flag - 1;
        while (end > command && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }
    }

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
        printf("Arka planda çalışan PID: %d\n", pid);
        fflush(stdout);

        // SIGCHLD sinyali için işleyici ayarla
        struct sigaction sa;
        sa.sa_handler = child_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
        sigaction(SIGCHLD, &sa, NULL);
    } else {
        perror("fork failed");
        exit(1);
    }
}

// 7) Pipe İşlemi: İki komut arasında veri akışını sağlayan işlev.
// $ ls | grep txt
// $ ls | grep c

void pipe_commands(char *command) {
    char *cmds[256];
    int num_cmds = 0;

    // Komutları '|' sembolüne göre ayrıştır
    char *token = strtok(command, "|");
    while (token != NULL) {
        cmds[num_cmds++] = token;
        token = strtok(NULL, "|");
    }

    int pipefds[2 * (num_cmds - 1)];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + 2 * i) == -1) {
            perror("pipe failed");
            exit(1);
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // İlk süreç değilse giriş için pipe bağla
            if (i > 0) {
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            // Son süreç değilse çıkış için pipe bağla
            if (i < num_cmds - 1) {
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }

            // Tüm pipe'ları kapat
            for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                close(pipefds[j]);
            }

            // Komut argümanlarını ayrıştır ve çalıştır
            char *args[256];
            int j = 0;
            char *arg_token = strtok(cmds[i], " \n");
            while (arg_token != NULL) {
                args[j++] = arg_token;
                arg_token = strtok(NULL, " \n");
            }
            args[j] = NULL;

            execvp(args[0], args);
            perror("exec failed");
            exit(1);
        } else if (pid < 0) {
            perror("fork failed");
            exit(1);
        }
    }

    // Tüm pipe'ları kapat
    for (int i = 0; i < 2 * (num_cmds - 1); i++) {
        close(pipefds[i]);
    }

    // Tüm çocuk süreçlerin bitmesini bekle
    for (int i = 0; i < num_cmds; i++) {
        wait(NULL);
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
            background_process(command);
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

