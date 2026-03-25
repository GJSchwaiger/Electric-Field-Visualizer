/********************************************************************************
 * File: main.cpp
 * Written By: George Schwaiger, 2/24/2026
 * Purpose: entry point for the 2D electric field visualization engine
 *          initializes the App, sets up GPU resources, and runs the main
 *          render loop until the user presses escape
 ********************************************************************************/

#include "App.h"

int main(){
    App app; //initialize application, calls default constructor
    app.triangleSetup(); //initialize triangle mesh and shaders
    app.circleSetup(); //initialze cirlce mesh and shaders

    while(!glfwWindowShouldClose(app.window)){ //main loop
        glClearColor(0,0,0,1); //set background color
        glClear(GL_COLOR_BUFFER_BIT); //clear frame buffer

        app.updateChargeFromCursor(); //update the positive charge to follow the cursor
        app.calculateElectricField(); //compute physics for each FieldVector
        app.flattenInstances(); //convert simulation data to gpu ready instance buffer
        app.drawTriangles(); //render electric field vectors
        app.drawCharges(); //render positive and negative charges

        glfwSwapBuffers(app.window); //output finished frame to screen
        glfwPollEvents(); //poll for new user input
    }

    return 0;
}