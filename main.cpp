#include <vector>
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

int width = 200;
int height = 200;
float dx = 1.0f;        // Grid spacing
float dt = 0.03f;       // Time step
float c = 1.0f;
float damping = 0.01f;         // Wave speed

std::vector<std::vector<float>> height_current(width, std::vector<float>(height));
std::vector<std::vector<float>> height_prev(width, std::vector<float>(height));
std::vector<std::vector<float>> height_next(width, std::vector<float>(height));


// fluid simulation logic
void update_wave(){
    for (int i = 1; i < width - 1; ++i){
        for (int j = 1; j < height - 1; ++j){
            float laplacian =
                height_current[i + 1][j] +
                height_current[i - 1][j] +
                height_current[i][j + 1] +
                height_current[i][j - 1] -
                4 * height_current[i][j];

            // Update using the wave equation
            height_next[i][j] = damping * (2 * height_current[i][j] - height_prev[i][j]) +
                                (c * c * dt * dt / (dx * dx)) * laplacian;
        }
    }

    // Swap buffers
    height_prev = height_current;
    height_current = height_next;
}

void add_disturbance(int x, int y, float height){
    height_current[x][y] = height;
}

void init_grid(){
    for(int i = 0; i < width; i++){
        for(int j = 0; j < height; j++){
            height_current[i][j] = 0.0f;
        }
    }
}

int main(){

    return 0;
}