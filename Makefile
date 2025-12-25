# Makefile for Raspberry Pi Physics Auth
CC = gcc
CFLAGS = -O3 -march=native -Wall -Wextra
LDFLAGS = -lm -lcurl

# Targets
all: test_physics

# Client requires libcurl (optional)
client: client.c physics_auth.c physics_auth.h
	$(CC) $(CFLAGS) -o auth_client client.c physics_auth.c $(LDFLAGS)

test_physics: test_physics.c physics_auth.c physics_auth.h
	$(CC) $(CFLAGS) -o test_physics test_physics.c physics_auth.c -lm

stm32_test: stm32_test.c physics_auth.c physics_auth.h
	$(CC) $(CFLAGS) -o stm32_test stm32_test.c physics_auth.c -lm -lrt

puf_test: puf_test.c physics_auth.c physics_auth.h
	$(CC) $(CFLAGS) -o puf_test puf_test.c physics_auth.c -lm

critical_validation: critical_validation.c physics_auth.c physics_auth.h
	$(CC) $(CFLAGS) -o critical_validation critical_validation.c physics_auth.c -lm -lrt

clean:
	rm -f auth_client test_physics

install: client
	sudo mkdir -p /usr/local/bin
	sudo mkdir -p /etc/physics_auth
	sudo cp auth_client /usr/local/bin/
	sudo chmod +x /usr/local/bin/auth_client
	@echo "Installed to /usr/local/bin/auth_client"
	@echo "Create secret file at /etc/physics_auth/secret.conf"

.PHONY: all clean install
