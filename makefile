PREFIX = /usr/local
CCFLAGS = -I./src -Iglm -o ao
LDFLAGS = `pkg-config --static --libs glfw3 glew`

ckube: main.c
	$(CCFLAGS) main.c $(LDFLAGS) -std=c99
	./ao

clean:
	rm ao

.PHONY: all ao clean

.PHONY: install
install: ao
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	sudo cp $< $(DESTDIR)$(PREFIX)/bin/ao

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/ao
