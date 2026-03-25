/********************************************************************************
 * File: App.h
 * Written By: George Schwaiger, 2/24/2026
 * Purpose: declares the App class, FieldVector and Charge structs, grid
 *          constants, and extern shader string declarations. defines all
 *          data owned by the application and the interface for simulation,
 *          rendering, and window management
 ********************************************************************************/

#ifndef APP_H 
#define APP_H 

    #include <glad/glad.h>
    #include <GLFW/glfw3.h>
    #include <omp.h>
    #include <cmath>
    #include <iostream>
    #include <numbers>
    #include <algorithm>

    #define _USE_MATH_DEFINES

    /********************************************************************************
     * Variable: triVertexShaderSrc
     * Written By: George Schwaiger, 2/24/2026
     * Purpose: vertex shader for rendering electric field arrows
     * Language: GLSL stored as a raw string literal, executed on the GPU
     * Notes: runs once per vertex per instance (3 vertices * INSTANCE_COUNT times)
     ********************************************************************************/
    extern const char* triVertexShaderSrc;

    /********************************************************************************
     * Variable: triFragmentShaderSrc
     * Written By: George Schwaiger, 2/24/2026
     * Purpose: fragment shader for coloring electric field arrows by magnitude
     * Language: GLSL stored as a raw string literal, executed on the GPU
     * Notes: mag is pre-normalized to 0-1 on the CPU in flattenInstances()
     ********************************************************************************/
    extern const char* triFragmentShaderSrc;

    /********************************************************************************
     * Variable: circleVertexShaderSrc
     * Written By: George Schwaiger, 2/24/2026
     * Purpose: vertex shader for rendering the point charge circles
     * Language: GLSL stored as a raw string literal, executed on the GPU
     * Notes: scales and translates unit circle vertices using uniforms
     ********************************************************************************/
    extern const char* circleVertexShaderSrc;

    /********************************************************************************
     * Variable: circleFragmentShaderSrc
     * Written By: George Schwaiger, 2/24/2026
     * Purpose: fragment shader for coloring the point charge circles
     * Language: GLSL stored as a raw string literal, executed on the GPU
     * Notes: color is passed as a uniform, red for positive, blue for negative
     ********************************************************************************/
    extern const char* circleFragmentShaderSrc;

    const int GRID_WIDTH = 125;                    // number of arrows per row
    const int GRID_HEIGHT = 50;                    // number of arrows per column
    const int INSTANCE_COUNT = GRID_WIDTH * GRID_HEIGHT; // total number of arrows
    const int CIRCLE_SEGMENTS = 64;                // number of triangles used to approximate each circle

    /********************************************************************************
     * Struct: FieldVector
     * Written By: George Schwaiger, 2/24/2026
     * Purpose: stores the position and electric field data for one grid point
     *          one of these exists for every cell in the GRID_WIDTH x GRID_HEIGHT array
     ********************************************************************************/
    struct FieldVector {
        double x;         // center of arrow x position in NDC space
        double y;         // center of arrow y position in NDC space
        double Ex;        // electric field x component at this point
        double Ey;        // electric field y component at this point
        double magnitude; // scalar field magnitude, used for color mapping
    };

    /********************************************************************************
     * Struct: Charge
     * Written By: George Schwaiger, 2/24/2026
     * Purpose: stores the position and value of one point charge
     *          value is signed: positive repels, negative attracts
     ********************************************************************************/
    struct Charge {
        double x;     // x position of the charge in NDC space
        double y;     // y position of the charge in NDC space
        double value; // signed magnitude of the charge, determines polarity
    };

    /********************************************************************************
     * Class: App
     * Written By: George Schwaiger, 2/24/2026
     * Purpose: owns all application state — the glfw window, OpenGL GPU resources,
     *          simulation data, and all methods to update and render the simulation
     ********************************************************************************/
    class App{ 
        public:
            // ---- window and glfw management ----
            GLFWmonitor* monitor;      // pointer to the primary monitor
            const GLFWvidmode* mode;   // pointer to the monitor's video mode (resolution, refresh rate)
            GLFWwindow* window;        // pointer to the glfw window
            
            // ---- simulation data ----
            FieldVector field[GRID_WIDTH][GRID_HEIGHT]; // 2D grid of field vectors, one per arrow
            Charge posCharge{0, 0,  1.0};               // positive point charge, follows the cursor
            Charge negCharge{0, 0, -1.0};               // negative point charge, fixed at origin
            float maxMag = 0.0f;                        // highest magnitude in the current frame, used for normalization

            // ---- triangle (arrow) rendering data ----
            GLuint triVS, triFS;                        // vertex and fragment shader objects
            GLuint triProgram;                          // linked shader program combining triVS and triFS
            GLuint triVAO;                              // vertex array object, stores attribute layout
            GLuint triVBO;                              // vertex buffer for the base triangle geometry
            GLuint instanceVBO;                         // instance buffer, updated every frame with field data
            float instanceData[INSTANCE_COUNT * 5];    // CPU-side instance buffer: [x, y, Ex, Ey, normMag] per arrow

            // ---- circle (charge) rendering data ----
            GLuint circleVS, circleFS; // vertex and fragment shader objects for circles
            GLuint circleProgram;      // linked shader program combining circleVS and circleFS
            GLuint circleVAO;          // vertex array object for circle geometry
            GLuint circleVBO;          // vertex buffer storing unit circle vertices

            /********************************************************************************
             * Function: App (constructor)
             * Written By: George Schwaiger, 2/24/2026
             * Returns: N/A
             * Parameters: none
             * Purpose: initializes glfw, creates a fullscreen window, and loads OpenGL
             *          via glad. sets up all callbacks. exits on any failure.
             ********************************************************************************/
            App(); 

            /********************************************************************************
             * Function: ~App (destructor)
             * Written By: George Schwaiger, 2/24/2026
             * Returns: N/A
             * Parameters: none
             * Purpose: frees all OpenGL GPU resources, destroys the window, and shuts
             *          down glfw cleanly when the program exits
             ********************************************************************************/
            ~App(); 

            /********************************************************************************
             * Function: updateChargeFromCursor
             * Written By: George Schwaiger, 2/24/2026
             * Returns: void
             * Parameters: none
             * Purpose: reads the cursor position from glfw and converts it from screen
             *          pixel coordinates to NDC space, then updates posCharge position
             ********************************************************************************/
            void updateChargeFromCursor(); 

            /********************************************************************************
             * Function: triangleSetup
             * Written By: George Schwaiger, 2/24/2026
             * Returns: void
             * Parameters: none
             * Purpose: compiles the triangle shader program and creates the VAO/VBOs
             *          for the base triangle mesh and the per-instance data buffer
             ********************************************************************************/
            void triangleSetup();

            /********************************************************************************
             * Function: circleSetup
             * Written By: George Schwaiger, 2/24/2026
             * Returns: void
             * Parameters: none
             * Purpose: generates unit circle vertices, uploads them to the GPU,
             *          and compiles the circle shader program
             ********************************************************************************/
            void circleSetup();

            /********************************************************************************
             * Function: drawTriangles
             * Written By: George Schwaiger, 2/24/2026
             * Returns: void
             * Parameters: none
             * Purpose: binds the triangle shader and issues one instanced draw call
             *          that renders all INSTANCE_COUNT field arrows in a single GPU call
             ********************************************************************************/
            void drawTriangles();

            /********************************************************************************
             * Function: drawCharges
             * Written By: George Schwaiger, 2/24/2026
             * Returns: void
             * Parameters: none
             * Purpose: renders the positive and negative point charges as filled circles
             *          negative is blue, positive is red, positions passed as uniforms
             ********************************************************************************/
            void drawCharges();

            /********************************************************************************
             * Function: flattenInstances
             * Written By: George Schwaiger, 2/24/2026
             * Returns: void
             * Parameters: none
             * Purpose: converts the 2D field array into a flat float buffer and uploads
             *          it to the GPU instance VBO. computes normalized magnitudes for
             *          color mapping. called every frame after calculateElectricField()
             ********************************************************************************/
            void flattenInstances();

            /********************************************************************************
             * Function: calculateElectricField
             * Written By: George Schwaiger, 2/24/2026
             * Returns: void
             * Parameters: none
             * Purpose: computes the electric field vector at every grid point using
             *          Coulomb's law and superposition of both point charges.
             *          results stored in the field array for use by flattenInstances()
             ********************************************************************************/
            void calculateElectricField();
    }; 

#endif