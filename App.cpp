/********************************************************************************
 * File: App.cpp
 * Written By: George Schwaiger, 2/24/2026
 * Purpose: implements the App class. contains all GLSL shader source strings,
 *          glfw callbacks, OpenGL setup, per-frame simulation logic, and
 *          rendering functions for the electric field arrow grid and point charges
 ********************************************************************************/

#include "App.h" 

/********************************************************************************
 * Variable: triVertexShaderSrc
 * Written By: George Schwaiger, 2/24/2026
 * Purpose: vertex shader for rendering electric field arrows
 * Language: GLSL stored as a raw string literal, executed on the GPU
 * Notes: runs once per vertex per instance (3 vertices * INSTANCE_COUNT times)
 *        each instance receives its own center position, field vector, and magnitude
 ********************************************************************************/
const char* triVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;           // base triangle vertex position (same for all instances)
layout(location = 1) in vec2 instanceCenter; // NDC position of this arrow's grid point
layout(location = 2) in vec2 instanceVector; // Ex, Ey components of the electric field at this point
layout(location = 3) in float instanceMag;   // pre-normalized magnitude, used for color in fragment shader

out float mag; // pass magnitude through to fragment shader for coloring

void main()
{
    // compute the rotation angle from the field vector
    // atan(x, y) gives the angle of the vector relative to the positive y-axis
    float angle = atan(instanceVector.x, instanceVector.y);

    // build a 2D rotation matrix from the angle
    // this rotates the base triangle to align with the field direction
    mat2 rot = mat2(cos(angle), -sin(angle),
                    sin(angle),  cos(angle));

    vec2 rotated = rot * aPos.xy;        // rotate the base triangle vertex
    vec2 finalPos = rotated + instanceCenter; // translate to this arrow's grid position

    gl_Position = vec4(finalPos, 0.0, 1.0); // output final clip-space position
    mag = instanceMag;                       // forward magnitude to fragment shader
}
)";

/********************************************************************************
 * Variable: triFragmentShaderSrc
 * Written By: George Schwaiger, 2/24/2026
 * Purpose: fragment shader for coloring electric field arrows by magnitude
 * Language: GLSL stored as a raw string literal, executed on the GPU
 * Notes: mag is pre-normalized to 0-1 on the CPU before being sent to the GPU
 *        arrows near charges are bright yellow, weak field regions appear grey
 ********************************************************************************/
const char* triFragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
in float mag; // normalized 0-1 magnitude, computed on CPU side in flattenInstances()

void main()
{
    vec3 grey   = vec3(0.3, 0.3, 0.3); // color for low magnitude field regions
    vec3 yellow = vec3(1.0, 1.0, 0.0); // color for high magnitude field regions near charges

    // linearly interpolate between grey and yellow based on normalized magnitude
    vec3 finalColor = mix(grey, yellow, mag);
    FragColor = vec4(finalColor, 1.0);
}
)";

/********************************************************************************
 * Variable: circleVertexShaderSrc
 * Written By: George Schwaiger, 2/24/2026
 * Purpose: vertex shader for rendering the point charge circles
 * Language: GLSL stored as a raw string literal, executed on the GPU
 * Notes: circle vertices are unit circle points, scaled and translated by uniforms
 *        aspect ratio correction is applied to prevent the circle from stretching
 ********************************************************************************/
const char* circleVertexShaderSrc = R"(
#version 330 core
layout(location=0) in vec2 aPos; // unit circle vertex, precomputed in circleSetup()
uniform vec2 center;             // NDC position of the charge to draw
uniform float scale;             // radius of the circle in NDC space
uniform float aspect;            // width/height ratio of the screen

void main()
{
    vec2 pos;
    pos.x = center.x + aPos.x * scale / aspect; // divide x by aspect to keep circle round on widescreen
    pos.y = center.y + aPos.y * scale;           // y is unmodified
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";

/********************************************************************************
 * Variable: circleFragmentShaderSrc
 * Written By: George Schwaiger, 2/24/2026
 * Purpose: fragment shader for coloring the point charge circles
 * Language: GLSL stored as a raw string literal, executed on the GPU
 * Notes: color is passed in as a uniform, red for positive, blue for negative
 ********************************************************************************/
const char* circleFragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 color; // RGB color passed in from drawCharges()
void main()
{
    FragColor = vec4(color, 1.0); // output solid color with full opacity
}
)";


/********************************************************************************
 * Function: error_callback
 * Written By: George Schwaiger, 2/24/2026
 * Returns: void
 * Parameters: error - glfw error code
 *             description - human readable c string explaining the error
 * Purpose: registered with glfw so it is called automatically on any glfw error
 ********************************************************************************/
static void error_callback(int error, const char* description){ 
    std::cout << "ERROR: " << error << std::endl; 
    std::cout << "DESCRIPTION: " << description << std::endl; 
    exit(-1); // unrecoverable, terminate the program
} 

/********************************************************************************
 * Function: key_callback
 * Written By: George Schwaiger, 2/24/2026
 * Returns: void
 * Parameters: window   - the glfw window that received the input
 *             key      - the keyboard key that was pressed or released
 *             scancode - platform specific scancode (not used)
 *             action   - GLFW_PRESS, GLFW_RELEASE, or GLFW_REPEAT
 *             mods     - modifier keys held (shift, ctrl, etc.) (not used)
 * Purpose: registered with glfw so it is called automatically on any key event
 ********************************************************************************/
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) // if the escape key is pressed
        glfwSetWindowShouldClose(window, true);        // signal the main loop to exit
} 

/********************************************************************************
 * Function: App (constructor)
 * Written By: George Schwaiger, 2/24/2026
 * Returns: N/A
 * Parameters: none
 * Purpose: initializes glfw, creates a fullscreen window, and loads OpenGL
 *          via glad. sets up all callbacks. exits on any failure.
 ********************************************************************************/
App::App(){
    if(!glfwInit()) // initialize glfw library, returns false on failure
        exit(-1); 

    // tell glfw we want an OpenGL 3.3 core profile context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); 
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 

    monitor = glfwGetPrimaryMonitor(); // get the primary monitor for fullscreen
    if (!monitor) { // if monitor is null, glfw couldn't find a display
        std::cout << "Failed to get primary monitor" << std::endl; 
        glfwTerminate(); 
        exit(-1); 
    }

    mode = glfwGetVideoMode(monitor); // get resolution and refresh rate of the monitor

    // create a fullscreen window at the monitor's native resolution
    // passing monitor as the 4th argument makes it fullscreen
    window = glfwCreateWindow(mode->width, mode->height, "title", monitor, nullptr); 

    if(!window){ // if window is null, creation failed
        glfwTerminate(); 
        exit(-1); 
    } 

    glfwMakeContextCurrent(window); // bind the OpenGL context to this window
    glfwSwapInterval(1);            // enable vsync, swap buffers once per monitor refresh
    glfwSetErrorCallback(error_callback); // register error handler
    glfwSetKeyCallback(window, key_callback); // register keyboard input handler

    // load all OpenGL function pointers via glad
    // glfwGetProcAddress provides the platform-specific loader
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ 
        std::cout << "FAILED TO INITIALIZE GLAD" << std::endl; 
        exit(-1); 
    } 

    // set the OpenGL viewport to cover the entire window
    glViewport(0, 0, mode->width, mode->height);
} 

/********************************************************************************
 * Function: ~App (destructor)
 * Written By: George Schwaiger, 2/24/2026
 * Returns: N/A
 * Parameters: none
 * Purpose: frees all OpenGL GPU resources, destroys the window, and shuts
 *          down glfw cleanly when the program exits
 ********************************************************************************/
App::~App(){ 
    // free triangle rendering GPU resources
    glDeleteVertexArrays(1, &triVAO);    // delete triangle vertex array object
    glDeleteBuffers(1, &triVBO);         // delete triangle vertex buffer
    glDeleteBuffers(1, &instanceVBO);    // delete instance data buffer
    glDeleteProgram(triProgram);         // delete linked triangle shader program

    // free circle rendering GPU resources
    glDeleteVertexArrays(1, &circleVAO); // delete circle vertex array object
    glDeleteBuffers(1, &circleVBO);      // delete circle vertex buffer
    glDeleteProgram(circleProgram);      // delete linked circle shader program

    glfwDestroyWindow(window); // destroy the glfw window and its context
    glfwTerminate();           // shut down glfw and free its resources
}

/********************************************************************************
 * Function: updateChargeFromCursor
 * Written By: George Schwaiger, 2/24/2026
 * Returns: void
 * Parameters: none
 * Purpose: reads the current cursor position from glfw and converts it from
 *          screen pixel coordinates to OpenGL NDC space (-1 to 1), then
 *          updates the positive charge position to follow the cursor
 ********************************************************************************/
void App::updateChargeFromCursor() { 
    double xpos, ypos; 
    glfwGetCursorPos(window, &xpos, &ypos); // get cursor in screen coords (top-left origin, pixels)

    // convert x: screen [0, width] -> NDC [-1, 1]
    posCharge.x = (2.0f * (float)xpos) / (float)mode->width - 1.0f; 

    // convert y: screen [0, height] -> NDC [-1, 1], flip because screen y grows downward
    posCharge.y = 1.0f - (2.0f * (float)ypos) / (float)mode->height; 
}

/********************************************************************************
 * Function: triangleSetup
 * Written By: George Schwaiger, 2/24/2026
 * Returns: void
 * Parameters: none
 * Purpose: compiles and links the triangle shader program, creates the VAO/VBO
 *          for the base triangle mesh, and creates the instance VBO that will
 *          be updated every frame with per-arrow position, vector, and magnitude
 ********************************************************************************/
void App::triangleSetup(){

    // base triangle vertices in local space, centered at origin
    // this same triangle is reused for every arrow via instanced rendering
    GLfloat triVertices[] = {
        -0.007f, -0.007f, 0.0f, // bottom left
         0.007f, -0.007f, 0.0f, // bottom right
         0.0f,    0.01f,  0.0f  // top center (tip of the arrow)
    };

    // -------- COMPILE AND LINK TRIANGLE SHADERS --------
    triVS = glCreateShader(GL_VERTEX_SHADER);           // create vertex shader object
    glShaderSource(triVS, 1, &triVertexShaderSrc, nullptr); // attach source code
    glCompileShader(triVS);                             // compile on the GPU driver

    triFS = glCreateShader(GL_FRAGMENT_SHADER);             // create fragment shader object
    glShaderSource(triFS, 1, &triFragmentShaderSrc, nullptr); // attach source code
    glCompileShader(triFS);                               // compile on the GPU driver

    triProgram = glCreateProgram();        // create an empty shader program
    glAttachShader(triProgram, triVS);     // attach compiled vertex shader
    glAttachShader(triProgram, triFS);     // attach compiled fragment shader
    glLinkProgram(triProgram);             // link both shaders into one executable program
    glDeleteShader(triVS);                 // shader objects no longer needed after linking
    glDeleteShader(triFS);

    // -------- SET UP BASE TRIANGLE GEOMETRY BUFFER --------
    glGenVertexArrays(1, &triVAO); // generate vertex array object to store attribute config
    glGenBuffers(1, &triVBO);      // generate vertex buffer for base triangle vertices
    glBindVertexArray(triVAO);     // bind VAO so all following attribute setup is recorded

    glBindBuffer(GL_ARRAY_BUFFER, triVBO);                              // bind triangle VBO
    glBufferData(GL_ARRAY_BUFFER, sizeof(triVertices), triVertices, GL_STATIC_DRAW); // upload, static since triangle never changes

    // location 0: base vertex position (xyz), 3 floats, stride 3 floats, offset 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // -------- SET UP INSTANCE DATA BUFFER --------
    // this buffer holds [x, y, Ex, Ey, mag] for each of the INSTANCE_COUNT arrows
    // it is updated every frame in flattenInstances() as the field changes
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(instanceData), nullptr, GL_DYNAMIC_DRAW); // nullptr since data comes later, dynamic because it changes every frame

    // location 1: instance center (xy), 2 floats, stride 5 floats, offset 0
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1); // advance once per instance, not once per vertex

    // location 2: instance field vector (Ex, Ey), 2 floats, stride 5, offset 2 floats
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1); // advance once per instance

    // location 3: instance magnitude, 1 float, stride 5, offset 4 floats
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(4*sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1); // advance once per instance

    glBindVertexArray(0); // unbind VAO to prevent accidental modification
}

/********************************************************************************
 * Function: circleSetup
 * Written By: George Schwaiger, 2/24/2026
 * Returns: void
 * Parameters: none
 * Purpose: generates unit circle vertices for rendering the point charges,
 *          uploads them to the GPU, and compiles the circle shader program
 ********************************************************************************/
void App::circleSetup(){
    // allocate array for circle vertices: 2 floats (x, y) per segment point
    GLfloat circleVerts[CIRCLE_SEGMENTS * 2];

    // compute evenly spaced points around a unit circle using parametric equations
    // x = cos(theta), y = sin(theta), theta from 0 to 2*PI
    #pragma omp parallel for // parallelize across threads for speed
    for (int i = 0; i < CIRCLE_SEGMENTS; i++){
        float theta = 2.0f * M_PI * i / CIRCLE_SEGMENTS; // angle for this segment
        circleVerts[i * 2 + 0] = cos(theta); // x component of unit circle point
        circleVerts[i * 2 + 1] = sin(theta); // y component of unit circle point
    }

    // -------- UPLOAD CIRCLE GEOMETRY TO GPU --------
    glGenVertexArrays(1, &circleVAO); // generate VAO for circle
    glGenBuffers(1, &circleVBO);      // generate VBO for circle vertices
    glBindVertexArray(circleVAO);     // bind VAO to record attribute setup

    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(circleVerts), circleVerts, GL_STATIC_DRAW); // static, circle shape never changes

    // location 0: circle vertex position (xy), 2 floats, stride 2 floats, offset 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0); // unbind VAO

    // -------- COMPILE AND LINK CIRCLE SHADERS --------
    circleVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(circleVS, 1, &circleVertexShaderSrc, nullptr);
    glCompileShader(circleVS);

    circleFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(circleFS, 1, &circleFragmentShaderSrc, nullptr);
    glCompileShader(circleFS);

    circleProgram = glCreateProgram();       // create empty program
    glAttachShader(circleProgram, circleVS); // attach vertex shader
    glAttachShader(circleProgram, circleFS); // attach fragment shader
    glLinkProgram(circleProgram);            // link into executable program
    glDeleteShader(circleVS);               // shader objects no longer needed after linking
    glDeleteShader(circleFS);
}

/********************************************************************************
 * Function: drawTriangles
 * Written By: George Schwaiger, 2/24/2026
 * Returns: void
 * Parameters: none
 * Purpose: binds the triangle shader program and issues a single instanced draw
 *          call that renders all INSTANCE_COUNT field arrows in one GPU call
 ********************************************************************************/
void App::drawTriangles(){
    glUseProgram(triProgram); // activate the triangle shader program

    // pass maxMag as a uniform so the shader can normalize magnitudes for coloring
    glUniform1f(glGetUniformLocation(triProgram, "maxMag"), maxMag);

    glBindVertexArray(triVAO); // bind the triangle VAO (base mesh + instance buffer)

    // draw all arrows in one call: 3 vertices per triangle, INSTANCE_COUNT instances
    // the GPU runs the vertex shader once per vertex per instance
    glDrawArraysInstanced(GL_TRIANGLES, 0, 3, INSTANCE_COUNT);
}

/********************************************************************************
 * Function: drawCharges
 * Written By: George Schwaiger, 2/24/2026
 * Returns: void
 * Parameters: none
 * Purpose: renders the positive and negative point charges as filled circles
 *          using TRIANGLE_FAN topology. negative is blue, positive is red.
 *          circle position, scale, and color are passed as uniforms each draw.
 ********************************************************************************/
void App::drawCharges(){
    // compute aspect ratio to pass into the circle vertex shader
    // prevents the circle from appearing as an ellipse on non-square screens
    float aspect = (float)mode->width / (float)mode->height;

    glUseProgram(circleProgram); // activate the circle shader program
    glBindVertexArray(circleVAO); // bind the circle VAO

    // uniforms shared by both circles
    glUniform1f(glGetUniformLocation(circleProgram, "scale"), 0.05f);   // radius in NDC space
    glUniform1f(glGetUniformLocation(circleProgram, "aspect"), aspect); // aspect ratio for x correction

    // ---- Draw Negative Charge (blue) ----
    glUniform2f(glGetUniformLocation(circleProgram, "center"),
                (float)negCharge.x, (float)negCharge.y); // set center to negative charge position
    glUniform3f(glGetUniformLocation(circleProgram, "color"),
                0.0f, 0.0f, 1.0f); // blue

    // TRIANGLE_FAN draws triangles from the first vertex to each consecutive pair,
    // forming a filled polygon — used here to fill the circle
    glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS);

    // ---- Draw Positive Charge (red) ----
    glUniform2f(glGetUniformLocation(circleProgram, "center"),
                (float)posCharge.x, (float)posCharge.y); // set center to positive charge position
    glUniform3f(glGetUniformLocation(circleProgram, "color"),
                1.0f, 0.0f, 0.0f); // red

    glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS);

    glBindVertexArray(0); // unbind VAO
}

/********************************************************************************
 * Function: flattenInstances
 * Written By: George Schwaiger, 2/24/2026
 * Returns: void
 * Parameters: none
 * Purpose: converts the 2D field array into a flat float array suitable for
 *          upload to the GPU instance buffer. also computes a normalized
 *          magnitude for each arrow used for color mapping in the shader.
 *          called every frame after calculateElectricField()
 ********************************************************************************/
void App::flattenInstances(){
    const double k = 8.9875517923e9; // Coulomb's constant, used to normalize magnitude back to pre-k range

    // ---- First Pass: find the maximum magnitude across all grid points ----
    // needed to normalize magnitudes relative to the strongest field present
    maxMag = 0.0f;
    for(int i = 0; i < GRID_WIDTH; i++)
        for(int j = 0; j < GRID_HEIGHT; j++)
            if(field[i][j].magnitude > maxMag) maxMag = (float)field[i][j].magnitude;

    // ---- Second Pass: pack field data into flat instance buffer ----
    // each arrow occupies 5 consecutive floats: [x, y, Ex, Ey, normMag]
    int idx = 0;
    for(int i = 0; i < GRID_WIDTH; i++){
        for(int j = 0; j < GRID_HEIGHT; j++){
            instanceData[idx++] = (float)field[i][j].x;  // grid point x position in NDC
            instanceData[idx++] = (float)field[i][j].y;  // grid point y position in NDC
            instanceData[idx++] = (float)field[i][j].Ex; // x component of electric field
            instanceData[idx++] = (float)field[i][j].Ey; // y component of electric field

            // normalize magnitude for color mapping:
            // divide by k to undo the Coulomb scaling, then apply log compression
            // to spread the color range — without this, arrows near charges dominate
            // and everything else appears at minimum brightness
            // pow(..., 2.0f) further compresses the high end to reduce yellow saturation
            float normMag = std::clamp((float)pow(log((float)(field[i][j].magnitude / k) + 1.0f), 2.0f), 0.0f, 1.0f);
            instanceData[idx++] = normMag;
        }
    }

    // upload the updated instance data to the GPU buffer
    // glBufferSubData is used instead of glBufferData to avoid reallocating the buffer each frame
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(instanceData), instanceData);
}

/********************************************************************************
 * Function: calculateElectricField
 * Written By: George Schwaiger, 2/24/2026
 * Returns: void
 * Parameters: none
 * Purpose: computes the electric field vector at every grid point using
 *          Coulomb's law and superposition. each grid point receives contributions
 *          from both the positive and negative point charges. results are stored
 *          in the field array for use by flattenInstances() each frame.
 ********************************************************************************/
void App::calculateElectricField(){
    const double epsilon = 1e-6;        // small threshold to avoid division by zero when a field point sits on a charge
    const double k = 8.9875517923e9;    // Coulomb's constant in N·m²/C²
    double widthInc  = 2.0 / (GRID_WIDTH  - 1); // NDC spans -1 to 1 (range = 2.0), divide evenly across columns
    double heightInc = 2.0 / (GRID_HEIGHT - 1); // same logic for rows

    #pragma omp parallel for // parallelize the outer loop across CPU threads for performance
    for (int i = 0; i < GRID_WIDTH; ++i) {
        for (int j = 0; j < GRID_HEIGHT; ++j) {

            // compute the NDC position of this grid point
            double x = -1.0 + i * widthInc;
            double y = -1.0 + j * heightInc;

            // accumulators for total electric field via superposition
            // each charge contributes independently, results are summed
            double Ex = 0.0;
            double Ey = 0.0;

            // ---- Positive Charge Contribution ----
            // r vector points FROM the charge TO the field point (Coulomb's law convention)
            double rx = x - posCharge.x;
            double ry = y - posCharge.y;
            double r2 = rx*rx + ry*ry; // squared distance, avoids an extra sqrt
            if (r2 > epsilon*epsilon){  // skip if field point is essentially on top of the charge
                double r = std::sqrt(r2);             // scalar distance
                double E = k * posCharge.value / r2;  // scalar field magnitude: E = kq/r²
                Ex += E * (rx / r); // x component: E scaled by r̂x (unit vector x component)
                Ey += E * (ry / r); // y component: E scaled by r̂y (unit vector y component)
            }                       // positive q means E points away from charge (repulsion)

            // ---- Negative Charge Contribution ----
            // same formula — negative q automatically flips E direction (attraction)
            rx = x - negCharge.x;
            ry = y - negCharge.y;
            r2 = rx*rx + ry*ry;
            if (r2 > epsilon*epsilon){
                double r = std::sqrt(r2);
                double E = k * negCharge.value / r2; // negCharge.value is negative, so E points toward the charge
                Ex += E * (rx / r);
                Ey += E * (ry / r);
            }

            // ---- Store Results in Field Array ----
            field[i][j].x  = x;                          // grid point NDC x position
            field[i][j].y  = y;                          // grid point NDC y position
            field[i][j].Ex = Ex;                         // total x component after superposition
            field[i][j].Ey = Ey;                         // total y component after superposition
            field[i][j].magnitude = std::sqrt(Ex*Ex + Ey*Ey); // scalar magnitude, used for color mapping
        }
    }
}