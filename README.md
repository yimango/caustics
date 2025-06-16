# Water Caustics Simulation

A real-time interactive water simulation with beautiful, realistic caustics effects using OpenGL, GLFW, and GLM.

![Water Caustics Demo](demo.gif)

## ✨ Features

- **Real-time Water Physics**: Interactive wave simulation with realistic propagation and damping
- **Dynamic Caustics**: GPU-accelerated caustics that react to every ripple and wave
- **Interactive Controls**: Click anywhere on the water surface to create disturbances
- **Multiple Render Passes**: Optimized rendering pipeline with caustics generation and surface rendering
- **Beautiful Visuals**: 
  - Gradient skybox background
  - Transparent water surface with realistic lighting
  - Pool-style bottom with animated caustic patterns
  - Warm, natural caustics lighting

## 🎮 Controls

- **Left Mouse Click**: Add water disturbances at cursor position
- **ESC**: Exit application

## 🛠️ Technical Implementation

### Water Physics
- **Wave Equation Solver**: Discrete 2D wave propagation using finite differences
- **Interactive Disturbances**: Real-time wave injection through mouse interaction
- **Damping System**: Prevents infinite oscillations for realistic behavior
- **200x200 Grid**: High-resolution simulation for detailed wave patterns

### Caustics Rendering
- **Surface Curvature Analysis**: Caustics intensity based on water surface deviation
- **Multi-layer Caustics**: Multiple sampling layers for complex, realistic patterns
- **Temporal Animation**: Time-based variation for natural caustic movement
- **Framebuffer Rendering**: Efficient GPU-based caustics generation

### Shader Pipeline
1. **Caustics Generation**: Analyzes water surface normals to generate caustic intensity
2. **Pool Bottom Rendering**: Applies caustics texture with warm lighting
3. **Water Surface**: Transparent rendering with Phong lighting
4. **Skybox**: Gradient background for atmospheric depth

## 📋 Requirements

- **OpenGL 3.3+** compatible graphics card
- **Windows** (currently configured for Windows with MinGW)
- **CMake 3.15+** for building
- **C++17** compatible compiler

## 🚀 Quick Start

### 1. Clone the Repository
```bash
git clone https://github.com/yourusername/water-caustics.git
cd water-caustics
```

### 2. Build the Project
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 3. Run the Simulation
```bash
./caustics.exe
```

## 📁 Project Structure

```
water-caustics/
├── main.cpp                 # Main application and all shaders
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── include/
│   └── glad/               # OpenGL function loader
│       ├── glad.h
│       └── glad.c
├── external/               # Dependencies
│   ├── glfw-3.4.bin.WIN64/ # GLFW window library
│   └── glm/                # Mathematics library
└── build/                  # Generated build files
    └── caustics.exe        # Compiled executable
```

## 🎨 Visual Features

### Water Simulation
- Realistic wave propagation with proper physics
- Interactive disturbance creation
- Smooth surface deformation

### Caustics Effects
- **Surface-Reactive**: Caustics change with every wave and ripple  
- **Natural Patterns**: Organic, flowing caustic shapes
- **Dynamic Animation**: Continuous movement and variation
- **Warm Lighting**: Realistic underwater light appearance

### Rendering Quality
- **Transparent Water**: Proper alpha blending for realistic water
- **Pool Aesthetics**: Blue pool-style bottom with subtle grid pattern  
- **Gradient Sky**: Beautiful blue gradient background
- **Optimized Performance**: Efficient multi-pass rendering

## ⚙️ Configuration

Key parameters in `main.cpp`:
```cpp
// Water simulation
int width = 200;           // Grid resolution
float waterScale = 2.0f;   // Visual scale factor
float damping = 0.01f;     // Wave damping

// Rendering
const float WATER_IOR = 1.33f;  // Water refraction index
const float BOTTOM_Z = -30.0f;  // Pool depth
```

## 🔧 Build System

The project uses CMake with:
- **Automatic Dependency Management**: GLFW binaries downloaded automatically
- **Custom GLAD Implementation**: Prevents OpenGL loader conflicts  
- **Cross-platform Support**: Configurable for different systems
- **Release Optimization**: Optimized builds for performance

## 🎯 Performance

- **Real-time Frame Rates**: 60+ FPS on modern hardware
- **GPU Acceleration**: Leverages graphics card for caustics generation
- **Efficient Memory Usage**: Optimized vertex and texture management
- **Smooth Interaction**: Responsive mouse controls with minimal latency

## 🐛 Troubleshooting

### Common Issues

**Graphics Issues:**
- Ensure OpenGL 3.3+ support
- Update graphics drivers
- Check for DirectX compatibility

**Build Issues:**  
- Verify CMake version (3.15+)
- Ensure C++17 compiler support
- Check MinGW installation on Windows

**Performance Issues:**
- Reduce grid resolution if needed
- Check graphics card compatibility
- Ensure adequate system RAM
