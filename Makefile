CC = gcc
CFLAGS = -Wall -g

myshell: shell.c
	$(CC) $(CFLAGS) shell.c -o myshell

clean:
	rm -f myshell *.o

test: myshell
	./myshell test_commands.txt
```

**Save:** Press `Cmd+S`

**IMPORTANT:** The lines under `myshell:`, `clean:`, and `test:` MUST start with a TAB, not spaces!

