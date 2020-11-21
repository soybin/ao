PREFIX = /usr/local
CCFLAGS = g++ -o ao -I./externals/imgui -I./externals/imgui/examples -I/usr/include/opencv4/
LDFLAGS = `pkg-config --static --libs glfw3 glew`
OPENCV_LFLAGS = -lopencv_core -lopencv_videoio
IMGUI = externals/imgui/imgui.cpp externals/imgui/imgui_demo.cpp externals/imgui/imgui_draw.cpp externals/imgui/imgui_widgets.cpp externals/imgui/examples/imgui_impl_opengl3.cpp externals/imgui/examples/imgui_impl_glfw.cpp

ao: src/ao.cpp
	$(CCFLAGS) src/ao.cpp src/shader.cpp $(IMGUI) $(OPENCV_LFLAGS) $(LDFLAGS)
	./ao
	rm ao

.PHONY: all ao

.PHONY: install
install: ao
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	sudo cp $< $(DESTDIR)$(PREFIX)/bin/ao

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/ao
