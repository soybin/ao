# ao「青」— cloud renderer

![rendered with ao](./res/img_01.jpg)

## what?
The sky in a binary file; ao「青」allows for real-time, physically-based rendering of clouds. You can create different types of clouds by modifying the many parameters. Furthermore, it allows for quick export of images (.png, .jpg, .ppm, and .bmp) and video (.avi and .mp4) of your clouds.  

[This](https://youtu.be/U4VzxylFkFw) is a video showing how ao works.  
[This](https://pablo.software/articles/2020/12/16/compute-clouds-from-randomness.html) is an article explaining the logic behind it.  

I hope you find this to be as interesting as I do.

## install
*Please, note this is not a finished version and there are certain features I plan to add in the future.*  
*Check dependencies below.*  
```
git clone https://github.com/soybin/ao
cd ao
make install
```

## dependencies
* [glew](https://github.com/nigels-com/glew)
* [glfw](https://github.com/glfw/glfw)
* [glm](https://github.com/g-truc/glm)
* [openCV](https://github.com/opencv/opencv)
