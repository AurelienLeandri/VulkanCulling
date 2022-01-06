LeoEngine
=========

Description
-----------

A personal C++ Vulkan project I have been working on for 5 months. It features indirect draw with both occlusion and frustum culling, and should be extended with other features after I get some time off :)

### Work in progress ###

* I would like to clarify the math part in the culling shader, and try to make occlusion culling a bit more stable. Some objects that are close to the camera aren't culled properly.
* I had to modify the culling shader quite a lot compared to resources I found (especially vkguide and Niagara, see the Acknowledgements section), and I hope to make it a bit cleaner and less buggy with time.

I still hope this will prove to be useful to some other people trying to implement culling and indirect draw.

How to use
----------

### Building ###

Use CMake-GUI or command line cmake to configure, and then build the project.
For building I used Visual Studio 2019, and the generator was Ninja (included in Visual Studio 2019).

### Running ###

**Launch *LeoEngine.exe* from the project root**. You can type --help for a more advanced use. You don't have to specify anything more; it will open with a big scene loading hundreds of sponzas by default. You also can specify an .scene filepath to load a specific scene. There are scene examples in resources/Models. Please use relative paths (starting from the project root) and launch LeoEngine.exe from the root as well (where it whould be located).

Once the renderer started, you can use the following commands:
* **WASD** for moving around, **spacebar** to go up, **left shift** to go down (like in Minecraft, yes)
* **F** disables frustum culling, you can enable it again by pressing F again
* **O** disables occlusion culling, you can enable it again by pressing O again
* **L** locks the point of view from which culling is computed to the current camera's position. You can then move around and see what has been culled from the point of view you just set. Press L again to re-tie the culling point of view to the camera.
* **T** makes all objects transparent to see occlusion culling in action without having to lock the camera. You can now happily see how it does not work perfectly! Right now this doubles the number of draw calls so the application will move much slower. I mainly use this feature for debugging.

Aknowledgements and nice resources
----------------------------------
I first went through vulkan-tutorial for some vulkan basics, then completed the knowledge I gained by going through this very useful book on vulkan, then went through vkguide which gives nice advice on architecture and best practices. This non vulkan-specific book also covers culling and is a very interesting read.

Although the code is now very different from these guides, MaterialBuilder (and associated classes) and VulkanInstance are still close to what you can find respectively in vkguide and vulkan-tutorial, so props to them!

Sascha Willem's and Nvidia's Vulkan Samples were also very useful to me.
Also this very nice (although a bit difficult to cherrypick from) video series about making a vulkan engine.
