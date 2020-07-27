PREFIX = /usr/local
CCFLAGS = g++ -o ao -I./externals/imgui -I./externals/imgui/examples
LDFLAGS = `pkg-config --static --libs glfw3 glew`
IMGUI = externals/imgui/imgui.cpp externals/imgui/imgui_draw.cpp externals/imgui/imgui_widgets.cpp externals/imgui/examples/imgui_impl_opengl3.cpp externals/imgui/examples/imgui_impl_glfw.cpp

ao: src/ao.c
	$(CCFLAGS) src/ao.c $(IMGUI) $(LDFLAGS)
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
