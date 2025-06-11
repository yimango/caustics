#include <vector>
#include <iostream>
using namespace std;

int width = 200;
int height = 200;
float dx = 1.0f;        // Grid spacing
float dt = 0.03f;       // Time step
float c = 1.0f;         // Wave speed

std::vector<std::vector<float>> height_current(width, std::vector<float>(height));
std::vector<std::vector<float>> height_prev(width, std::vector<float>(height));
std::vector<std::vector<float>> height_next(width, std::vector<float>(height));



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
            height_next[i][j] = 2 * height_current[i][j] - height_prev[i][j] +
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

int main(){
    // Initialize with a single point disturbance
    height_current[100][100] = 1.0f;
    
    // Run multiple iterations
    for(int iteration = 0; iteration < 10; iteration++) {
        update_wave();
        
        // Print the current state
        cout << "Iteration " << iteration << ":" << endl;
        for (int i = 95; i < 105; ++i){  // Print a smaller region around the disturbance
            for (int j = 95; j < 105; ++j){
                cout << height_current[i][j] << "\t";
            }
            cout << endl;
        }
        cout << endl;
    }
    return 0;
}