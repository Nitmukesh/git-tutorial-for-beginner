CC = $(CROSS_COMPILE)gcc -std=gnu99

INCLUDE = -I$(STAGING_DIR)/usr/include -I.
CFLAG = -Wall -Wmissing-prototypes -Wstrict-prototypes -fexceptions -lnmp_mcuioservice -lpthread -liw

wifispeaker-factory-test: main.o
	$(CC) $(INCLUDE) $(CFLAG) -o $@ $^

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAH) -c $<

clean:
	rm *.o wifispeaker-factory-test

.PHONY: clean 

