#39.GRUP:
#B221210028 Beyazıt Han Bayraktar
#B221210034 Berat Yılmaz
#B221210373 Ali Aydın
#B221210308 Mustafa Erdoğan
#G211210002 Gökberk Atasoy

# Derleyici ve bayraklar
CC = gcc
CFLAGS = -Wall -Werror -g

# Çıktı dosyası
TARGET = OS_V1

# Tüm hedefler
all: $(TARGET)

# Programın derlenmesi
$(TARGET): OS_V1.c
	$(CC) $(CFLAGS) -o $(TARGET) OS_V1.c

# Temizlik işlemi
clean:
	rm -f $(TARGET)