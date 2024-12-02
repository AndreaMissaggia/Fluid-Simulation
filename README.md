## Simulazione dei fluidi in tempo reale  

Questo progetto implementa un motore per la simulazione dei fluidi in tempo reale utilizzando le API Vulkan.
La simulazione è basata sul metodo di Stam 'Stable Fluids'.

## Dipendenze
Questo progetto utilizza le seguenti librerie:

- [Vulkan Memory Allocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) - distribuito sotto licenza MIT.
- [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) - distribuito sotto licenza MIT.

Questo progetto dipende dai seguenti pacchetti:

- g++
- sdl2
- glm
- vulkan (GPU vendor specific)
- vulkan-headers
- vulkan-validation-layers
- glslang

## Compilazione ed esecuzione (Linux)

```bash
mkdir project
cd project
git clone https://github.com/AndreaMissaggia/Fluid-Simulation.git
cd Fluid-Simulation
make run
