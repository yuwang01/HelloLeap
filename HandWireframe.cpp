#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include <string.h>
#include "Leap.h"
#include "maths_funcs.h"

using namespace Leap;

#define GL_LOG_FILE "GL.log"

#define SUCCESS 1
#define FAILED 0

#define true 1
#define false 0

static int useLeap = 0;

const std::string fingerNames[] = {"Thumb", "Index", "Middle", "Ring", "Pinky"};
const std::string boneNames[] = {"Metacarpal", "Proximal", "Middle", "Distal"};
const std::string stateNames[] = {"STATE_INVALID", "STATE_START", "STATE_UPDATE", "STATE_END"};

int g_gl_width = 800;
int g_gl_height = 800;

float cam_pos[3];
float cam_yaw = 0.0f;

float kViewWidth = 40.;
float kViewHeight;
float kViewDepth = 40.;

GLint fingers_view_mat_location;
GLint fingers_proj_mat_location;

static double previous_seconds;
static double elapsed_seconds;
static int frame_count;

static unsigned int positionIndexHandle;

static unsigned int shader_program;
static unsigned int vao = 0;

static GLuint pointInd[] = {
    0, 1,
    1, 2,
    2, 0
};

////////////////////////////////////////////////////////////////////////////////
static Controller controller;
static float* fingers;
static float* fingercolors;
static unsigned int fingersVBO[3];
static unsigned int fingersVAO;
static unsigned int fingersProgram;
static unsigned int fingersVS;
static unsigned int fingersFS;

static unsigned int fingerspositionBufferHandle;
////////////////////////////////////////////////////////////////////////////////
void glfw_window_resize_callback(GLFWwindow* window, int width, int height) {
    g_gl_width = width;
    g_gl_height = height;

    glViewport(0, 0, g_gl_width, g_gl_height);
}

bool gl_log(const char* message, const char* filename, int line) {
    FILE* file = fopen(GL_LOG_FILE, "a+");
    if (!file) {
        fprintf(stderr, "ERROR: could not open %s for logging\n", GL_LOG_FILE);
        return false;
    }

    // fprintf(file, "%s: %i %s\n", filename, line, message);
    fprintf(file, "%s\n", message); 
    fclose(file);
    return true;
}

void glfw_error_callback(int error, const char* description) {
    fputs(description, stderr);
    gl_log(description, __FILE__, __LINE__);
}

void log_gl_params () {
    GLenum params[] = {
        GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
        GL_MAX_CUBE_MAP_TEXTURE_SIZE,
        GL_MAX_DRAW_BUFFERS,
        GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
        GL_MAX_TEXTURE_IMAGE_UNITS,
        GL_MAX_TEXTURE_SIZE,
        GL_MAX_VARYING_FLOATS,
        GL_MAX_VERTEX_ATTRIBS,
        GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
        GL_MAX_VERTEX_UNIFORM_COMPONENTS,
        GL_MAX_VIEWPORT_DIMS,
        GL_STEREO
    };

    const char* names[] = {
        "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS",
        "GL_MAX_CUBE_MAP_TEXTURE_SIZE",
        "GL_MAX_DRAW_BUFFERS",
        "GL_MAX_FRAGMENT_UNIFORM_COMPONENTS",
        "GL_MAX_TEXTURE_IMAGE_UNITS",
        "GL_MAX_TEXTURE_SIZE",
        "GL_MAX_VARYING_FLOATS",
        "GL_MAX_VERTEX_ATTRIBS",
        "GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS",
        "GL_MAX_VERTEX_UNIFORM_COMPONENTS",
        "GL_MAX_VIEWPORT_DIMS",
        "GL_STEREO"
    };

    gl_log ("GL Context Params:", __FILE__, __LINE__);

    char msg[256];
    // integers - only works if the order is 0-10 integer return types
    for (int i = 0; i < 10; i++) {
        int v = 0;
        glGetIntegerv (params[i], &v);
        sprintf (msg, "%s %i", names[i], v);
        gl_log (msg, __FILE__, __LINE__);
    }
    
    // others
    int v[2];
    v[0] = v[1] = 0;
    glGetIntegerv (params[10], v);
    sprintf (msg, "%s %i %i\n", names[10], v[0], v[1]);
    gl_log (msg, __FILE__, __LINE__);

    unsigned char s = 0;
    glGetBooleanv (params[11], &s);
    sprintf (msg, "%s %i\n", names[11], (unsigned int)s);
    gl_log (msg, __FILE__, __LINE__);
    gl_log ("-----------------------------\n", __FILE__, __LINE__);
}

void print_shader_info_log(GLuint shader_index)
{
    int max_length = 2048;

    int actual_length = 0;

    char log[2048];
    glGetShaderInfoLog(shader_index, max_length, &actual_length, log);

    if (actual_length == 0)
        fprintf(stderr, "Success: shader index %u\n", shader_index);
    else
        fprintf(stderr, "Shader info log for shader index %u: %s\n", shader_index, log);
    }

    char* readShader(const char* filename)
    {
        FILE* file = fopen(filename, "r");
        if (!file)
        {
        fprintf(stderr, "Cannot open file %s\n", filename);
        exit(1);
    }

    char* shaderStr;
    long fileLen;

    fseek(file, 0, SEEK_END);
    fileLen = ftell(file);
    rewind(file);
    shaderStr = (char*)malloc((fileLen + 1) * sizeof(char));
    fread(shaderStr, sizeof(char), fileLen, file);
    fclose(file);
    shaderStr[fileLen] = 0;

    return shaderStr;
}

void fps(GLFWwindow* window) {  
    double current_seconds = glfwGetTime();
    elapsed_seconds = current_seconds - previous_seconds;
    if (elapsed_seconds > .25) {
        previous_seconds = current_seconds;
        double FPS = (double)frame_count/elapsed_seconds;
        char tmp[128];

        sprintf(tmp, "fps: %.2f", FPS);
        glfwSetWindowTitle(window, tmp);
        frame_count = 0;
    }
    
    frame_count++;
}

int InitOpenGL()
{
    const GLubyte* renderer = glGetString (GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString (GL_VERSION); // version as a string
    printf ("Renderer: %s\n", renderer);
    printf ("OpenGL version supported %s\n", version);
    log_gl_params();

    // tell GL to only draw onto a pixel if the shape is closer to the viewer
    glEnable (GL_DEPTH_TEST); // enable depth-testing
    glDepthFunc (GL_LESS); // depth-testing interprets a smaller value as "closer"
    glEnable(GL_CULL_FACE); // cull face
    glCullFace(GL_BACK); // cull back face
    glFrontFace(GL_CW); // clock-wise order vertices make the front face
    glEnable (GL_DEPTH_TEST); // enable depth-testing
    glDepthFunc (GL_LESS); // depth-testing interprets a smaller value as "closer"
    
    ////////////////////////////////////////////////////////////////////////////////
    const char* fingers_vertex_shader = readShader("shaders/fingers.vert");
    const char* fingers_fragment_shader = readShader("shaders/fingers.frag");

    fingersVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(fingersVS, 1, &fingers_vertex_shader, NULL);
    glCompileShader(fingersVS);
    print_shader_info_log(fingersVS);

    fingersFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fingersFS, 1, &fingers_fragment_shader, NULL);
    glCompileShader(fingersFS);
    print_shader_info_log(fingersFS);

    fingersProgram = glCreateProgram();
    glAttachShader(fingersProgram, fingersVS);
    glAttachShader(fingersProgram, fingersFS);

    glLinkProgram(fingersProgram);

    glBindAttribLocation(fingersProgram, 0, "fingerPos");
    ////////////////////////////////////////////////////////////////////////////////
    glGenBuffers(1, fingersVBO);
    fingerspositionBufferHandle = fingersVBO[0];
    
    fingers = (float*)malloc(sizeof(float) * 120);
    
    glBindBuffer(GL_ARRAY_BUFFER, fingerspositionBufferHandle);
    glBufferData(GL_ARRAY_BUFFER, 120 * sizeof(float), fingers, GL_STATIC_DRAW);

    glGenVertexArrays(1, &fingersVAO);
    glBindVertexArray(fingersVAO);

    glEnableVertexAttribArray(0);
    
    glBindBuffer(GL_ARRAY_BUFFER, fingerspositionBufferHandle);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    ////////////////////////////////////////////////////////////////////////////////
    // create projection matrix and transformation matrix
    cam_pos[0] = 0.0f;
    cam_pos[1] = 0.0f;
    // cam_pos[2] = 1.2*kViewDepth;
    cam_pos[2] = 1.5*kViewDepth; // 1/r*r; and 1/r

    float near = .1f;
    float far = 300.0f;
    float fov = 75.0f;
    float aspect = (float)g_gl_width / (float)g_gl_height;

    float range = tan(fov * .5f) * near;
    float Sx = (2.0f * near) / (range * aspect + range * aspect);
    float Sy = near / range;
    float Sz = -(far + near) / (far - near);
    float Pz = -(2.0f * far * near) / (far - near);
    GLfloat proj_mat[] = {
        Sx, 0.0f, 0.0f, 0.0f,
        0.0f, Sy, 0.0f, 0.0f,
        0.0f, 0.0f, Sz, -1.0f,
        0.0f, 0.0f, Pz, 0.0f
    };

    mat4 T = translate (identity_mat4 (), vec3 (-cam_pos[0], -cam_pos[1], -cam_pos[2]));
    mat4 R = rotate_y_deg (identity_mat4 (), cam_yaw);
    mat4 view_mat = T * R;

    fingers_view_mat_location = glGetUniformLocation(fingersProgram, "view");
    fingers_proj_mat_location = glGetUniformLocation(fingersProgram, "proj");
    glUseProgram(fingersProgram);
    glUniformMatrix4fv(fingers_view_mat_location, 1, GL_FALSE, view_mat.m);
    glUniformMatrix4fv(fingers_proj_mat_location, 1, GL_FALSE, proj_mat);

    ////////////////////////////////////////////////////////////////////////////////
    
    return SUCCESS;
}

int InitLeap()
{
    int connected = 0;
    while (controller.isConnected() != true)
    {
        if (connected == 0)
        {
            std::cout << "InitLeap: Connecting to controller" << std::endl;
            connected++;
        }
        else if (connected >= 20)
        {
            std::cout << "InitLeap: Failed to connect to controller" << std::endl;
            return FAILED;
        } 
        else
        {
            connected++;
        }
        // wait until Controller.isConnected() evaluates to true
        //...
    }

    if (controller.isConnected() == true)
    {
        std::cout << "InitLeap: Success - connected to controller" << std::endl;
        return SUCCESS;
    }
    else
    {
        std::cout << "InitLeap: Failed to connect to controller" << std::endl;
        return FAILED;
    }
}

void retrieveFrame(const Controller& controller)
{
    const Frame frame = controller.frame();
    std::cout << "Frame id: " << frame.id()
              << ", timestamp: " << frame.timestamp()
              << ", hands: " << frame.hands().count()
              << ", fingers: " << frame.fingers().count()
              << ", tools: " << frame.tools().count()
              << ", gestures: " << frame.gestures().count() << std::endl;

    HandList hands = frame.hands();
    for (HandList::const_iterator hl = hands.begin(); hl != hands.end(); ++hl) {
        const Hand hand = *hl;
        std::string handType = hand.isLeft() ? "Left hand" : "Right hand";
        std::cout << std::string(2, ' ') << handType << ", id: " << hand.id()
                  << ", palm position: " << hand.palmPosition() << std::endl;

        const Vector normal = hand.palmNormal();
        const Vector direction = hand.direction();

        std::cout << std::string(2, ' ') << "pitch: " << direction.pitch() * RAD_TO_DEG << " degrees, "
                  << "roll: " << normal.roll() * RAD_TO_DEG << " degrees, "
                  << "yaw: " << direction.yaw() * RAD_TO_DEG << " degrees" << std::endl;

        Arm arm = hand.arm();
        std::cout << std::string(2, ' ') << "Arm direction: " << arm.direction()
                  << " wrist position: " << arm.wristPosition()
                  << " elbow position: " << arm.elbowPosition() << std::endl;

        const FingerList finger = hand.fingers();
        int fc = 0;
        for (FingerList::const_iterator fl = finger.begin(); fl != finger.end(); ++fl) {
            const Finger finger = *fl;
            std::cout << std::string(4, ' ') << finger.id()
                      << " fingers, id: " << finger.id()
                      << ", length: " << finger.length()
                      << "mm, width: " << finger.width() << std::endl;
            for (int b = 0; b < 4; ++b) {
                Bone::Type boneType = static_cast<Bone::Type>(b);
                Bone bone = finger.bone(boneType);
                fingers[fc] = -bone.prevJoint().x/20.0;
                fingers[fc+1] = -bone.prevJoint().y/20.0;
                fingers[fc+2] = -bone.prevJoint().z/20.0;
                fingers[fc+3] = -bone.nextJoint().x/20.0;
                fingers[fc+4] = -bone.nextJoint().y/20.0;
                fingers[fc+5] = -bone.nextJoint().z/20.0;

                std::cout << std::string(6, ' ') << boneNames[boneType]
                          << " bone, start: " << bone.prevJoint()
                          // << " DEBUG: bone, start: " << "(" << fingers[fc] << ", " << fingers[fc+1] << ", " << fingers[fc+2] << ")"
                          << ", end: " << bone.nextJoint()
                          // << " DEBUG: bone, end: " << "(" << fingers[fc+3] << ", " << fingers[fc+4] << ", " << fingers[fc+5] << ")"
                        << ", direction: " << bone.direction() << std::endl;
                fc += 6;
            }
        }
    }

    const ToolList tools = frame.tools();
    for (ToolList::const_iterator tl = tools.begin(); tl != tools.end(); ++tl) {
        const Tool tool = *tl;
        std::cout << std::string(2, ' ') << "Tool, id: " << tool.id()
                  << ", position: " << tool.tipPosition()
                  << ", direction: " << tool.direction() << std::endl;
    }

    const GestureList gestures = frame.gestures();
    for (int g = 0; g < gestures.count(); ++g) {
        Gesture gesture = gestures[g];

        switch (gesture.type()) {
            case Gesture::TYPE_CIRCLE:
            {
                CircleGesture circle = gesture;
                std::string clockwiseness;

                if (circle.pointable().direction().angleTo(circle.normal()) <= PI/2) {
                    clockwiseness = "clockwise";
                }
                else {
                    clockwiseness = "counterclockwise";
                }

                float sweptAngle = 0;
                if (circle.state() != Gesture::STATE_START) {
                    CircleGesture previousUpdate = CircleGesture(controller.frame(1).gesture(circle.id()));
                    sweptAngle = (circle.progress() - previousUpdate.progress()) * 2 * PI;
                }
                std::cout << std::string(2, ' ')
                          << "Circle id: " << gesture.id()
                          << ", state: " << stateNames[gesture.state()]
                          << ", progress: " << circle.progress()
                          << ", radius: " << sweptAngle * RAD_TO_DEG
                          << ", " << clockwiseness << std::endl;
                break;
            }

            case Gesture::TYPE_SWIPE:
            {
                SwipeGesture swipe = gesture;
                std::cout << std::string(2, ' ')
                          << "Swipe id: " << gesture.id()
                          << ", state: " << stateNames[gesture.state()]
                          << ", direction: " << swipe.direction()
                          << ", speed: " << swipe.speed() << std::endl;
                break;
            }

            case Gesture::TYPE_KEY_TAP:
            {
                KeyTapGesture tap = gesture;
                std::cout << std::string(2, ' ')
                          << "Key Tap id: " << gesture.id()
                          << ", state: " << stateNames[gesture.state()]
                          << ", position: " << tap.position()
                          << ", direction: " << tap.direction() << std::endl;
                break;
            }

            case Gesture::TYPE_SCREEN_TAP:
            {
                ScreenTapGesture screentap = gesture;
                std::cout << std::string(2, ' ')
                          << "Screen Tap id: " << gesture.id()
                          << ", state: " << stateNames[gesture.state()]
                          << ", position: " << screentap.position()
                          << ", direction: " << screentap.direction() << std::endl;
                break;
            }

            default:
                std::cout << std::string(2, ' ') << "Unknown gesture type." << std::endl;
                break;
        }
    }

    if (!frame.hands().isEmpty() || !gestures.isEmpty()) {
        std::cout << std::endl;
    }
}

void printFingers()
{
    int i;
    for (i = 0; i < 120; i+=6)
    {
        fprintf(stdout, "(%f, %f, %f) - (%f, %f, %f)\n", fingers[i], fingers[i+1], fingers[i+2], fingers[i+3], fingers[i+4], fingers[i+5]);
    }
}

int main(int argc, char** argv) {

    char message[256];
    sprintf(message, "starting GLFW %s", glfwGetVersionString());
    assert(gl_log(message, __FILE__, __LINE__));
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        fprintf (stderr, "ERROR: could not start GLFW3\n");
        return 1;
    } 

    // uncomment these lines if on Apple OS X
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(g_gl_width, g_gl_height, "Leap Motion - Hand Wireframe", NULL, NULL);

    if (!window) {
        fprintf (stderr, "ERROR: could not open window with GLFW3\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetWindowSizeCallback(window, glfw_window_resize_callback);

    glewExperimental = GL_TRUE;
    glewInit();

    if (InitOpenGL() != SUCCESS)
    {
        fprintf(stderr, "OpenGL initialization failed. Exit.");
        exit(1);
    }

    if (InitLeap() != SUCCESS)
    {
        useLeap = false;
    }
    else
    {
        useLeap = true;
    }

    while (!glfwWindowShouldClose(window)) {
        
        fps(window);

        if (useLeap == true)
        {
            // fill the vertex array for fingers
            retrieveFrame(controller);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(fingersProgram);
        glBindVertexArray(fingersVAO);
        glBindBuffer(GL_ARRAY_BUFFER, fingerspositionBufferHandle);
        glBufferData(GL_ARRAY_BUFFER, 120 * sizeof(float), fingers, GL_STATIC_DRAW);
        
        glDrawArrays(GL_LINES, 0, 40);
        glfwPollEvents();
        glfwSwapBuffers(window);

        if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window, 1);
        }
    }

    glfwTerminate();
    return 0;
}