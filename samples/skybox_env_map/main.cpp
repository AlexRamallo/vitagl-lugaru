// Based on https://learnopengl.com/Advanced-OpenGL/Cubemaps
#include <vector>
#include <stdio.h>
#include <vitasdk.h>
#include <vitaGL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <libtoloader.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Number of shader sets available
#define SHADERS_NUM 2

// Analogs deadzone
#define ANALOGS_DEADZONE 30

// Macro to check if a button has been pressed
#define CHECK_BTN(x) ((pad.buttons & x) && (!(old_buttons & x)))

// Global Up and Front vectors
glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 front = glm::vec3(0.0f, 1.0f, 0.0f);

// Camera setup
const float camera_sensitivity = 0.01f;
GLboolean can_fly = GL_FALSE;
glm::vec3 camera_pos = glm::vec3(0.0f, 0.0f, 7.0f); // Camera position
glm::vec3 camera_front; // View direction
glm::vec3 camera_up; // Up vector for the camera
glm::vec3 camera_orientation = glm::vec3(-90.0f, 0.0f, 0.0f); // Yaw, Pitch, Roll
glm::vec3 camera_right; // Right vector for the camera

void update_camera() {
	// Based on https://learnopengl.com/#!Getting-started/Camera
	camera_front = glm::normalize(glm::vec3(cos(glm::radians(camera_orientation.x)) * cos(glm::radians(camera_orientation.y)),
		sin(glm::radians(camera_orientation.y)), sin(glm::radians(camera_orientation.x)) * cos(glm::radians(camera_orientation.y))));
	front = glm::vec3(camera_front.x, 0.0f, camera_front.z);
	camera_right = glm::normalize(glm::cross(camera_front, up));
	camera_up = glm::normalize(glm::cross(camera_right, camera_front));
}

// Movement setup
const float movement_speed = 1.0f;

// Available illumination models
enum {
	MIRROR,
	SKYBOX
};

// Shaders and programs
GLuint vshaders[SHADERS_NUM];
GLuint fshaders[SHADERS_NUM];
GLuint programs[SHADERS_NUM];

// Uniforms locations
GLint modelMatrixLoc[SHADERS_NUM];
GLint inverseModelMatrixLoc[SHADERS_NUM];
GLint viewMatrixLoc[SHADERS_NUM];
GLint projectionMatrixLoc[SHADERS_NUM];
GLint cameraPosLoc[SHADERS_NUM];

// Initialize sceMsgDialog widget with a given message text
int init_msg_dialog(const char *msg) {
	SceMsgDialogUserMessageParam msg_param;
	memset(&msg_param, 0, sizeof(msg_param));
	msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	msg_param.msg = (SceChar8 *)msg;

	SceMsgDialogParam param;
	sceMsgDialogParamInit(&param);
	_sceCommonDialogSetMagicNumber(&param.commonParam);
	param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	param.userMsgParam = &msg_param;

	return sceMsgDialogInit(&param);
}

// Gets current state for sceMsgDialog running widget
int get_msg_dialog_result(void) {
	if (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return 0;
	sceMsgDialogTerm();
	return 1;
}

// Draws an error message on screen and force closes the app after user input
void fatal_error(const char *fmt, ...) {
	va_list list;
	char string[512];

	va_start(list, fmt);
	vsnprintf(string, sizeof(string), fmt, list);
	va_end(list);
	
	init_msg_dialog(string);

	while (!get_msg_dialog_result()) {
		glClear(GL_COLOR_BUFFER_BIT);
		vglSwapBuffers(GL_TRUE);
	}

	sceKernelExitProcess(0);
	while (1);
}

// Loads a shader from filesystem
void loadShader(const char *name, int type) {
	// Load vertex shader from filesystem
	char fname[256];
	sprintf(fname, "app0:%s.vert", name);
	FILE *f = fopen(fname, "r");
	if (!f)
		fatal_error("Cannot open %s", fname);
	fseek(f, 0, SEEK_END);
	int32_t vsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *vshader = (char *)malloc(vsize);
	fread(vshader, 1, vsize, f);
	fclose(f);
	
	// Load fragment shader from filesystem
	sprintf(fname, "app0:%s.frag", name);
	f = fopen(fname, "r");
	if (!f)
		fatal_error("Cannot open %s", fname);
	fseek(f, 0, SEEK_END);
	int32_t fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *fshader = (char *)malloc(fsize);
	fread(fshader, 1, fsize, f);
	fclose(f);
	
	// Create required shaders and program
	vshaders[type] = glCreateShader(GL_VERTEX_SHADER);
	fshaders[type] = glCreateShader(GL_FRAGMENT_SHADER);
	programs[type] = glCreateProgram();
	
	// Compiling vertex shader
	glShaderSource(vshaders[type], 1, &vshader, &vsize);
	glCompileShader(vshaders[type]);
	
	// Compiling fragment shader
	glShaderSource(fshaders[type], 1, &fshader, &fsize);
	glCompileShader(fshaders[type]);
	
	// Attaching shaders to final program
	glAttachShader(programs[type], vshaders[type]);
	glAttachShader(programs[type], fshaders[type]);
	
	// Binding attrib locations for the given shaders
	glBindAttribLocation(programs[type], 0, "position");
	if (type == MIRROR)
		glBindAttribLocation(programs[type], 1, "normal");
	
	// Linking program
	glLinkProgram(programs[type]);
	
	// Getting uniforms locations for the given shaders
	if (type == MIRROR) {
		modelMatrixLoc[type] = glGetUniformLocation(programs[type], "modelMatrix");
		inverseModelMatrixLoc[type] = glGetUniformLocation(programs[type], "inverseModelMatrix");
		cameraPosLoc[type] = glGetUniformLocation(programs[type], "camera_pos");
	}
	viewMatrixLoc[type] = glGetUniformLocation(programs[type], "viewMatrix");
	projectionMatrixLoc[type] = glGetUniformLocation(programs[type], "projectionMatrix");
	
	// Deleting temporary buffers
	free(fshader);
	free(vshader);
}

// Draws a model
void drawModel(to_model *mdl, GLboolean is_skybox) {
	glEnableVertexAttribArray(0);
	if (is_skybox)
		glDisableVertexAttribArray(1);
	else	
		glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, mdl->pos);
	if (!is_skybox)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, mdl->normals);
	glDrawArrays(GL_TRIANGLES, 0, mdl->num_vertices);
}

int main() {
	// Initializing graphics device
	vglInit(0x800000);
	
	// Enabling sampling for the analogs
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	
	// Setting screen clear color
	glClearColor(0.26f, 0.46f, 0.98f, 1.0f);
	
	// Enabling depth test
	glEnable(GL_DEPTH_TEST);
	
	// Projection matrix: FOV angle, aspect ratio, near and far planes
	glm::mat4 projection = glm::perspective(45.0f, 960.0f / 544.0f, 0.1f, 10000.0f);
	
	// Initializing model matrices for our objects to identity
	glm::mat4 bunnyModelMatrix = glm::mat4(1.0f);
	
	// Default values for spinning state, wireframe mode and pressed buttons bitmask
	uint32_t old_buttons = 0;
	GLboolean spinning = GL_TRUE;
	GLboolean wireframe = GL_FALSE;
	
	// Default values for timing calculations
	GLfloat deltaTime = 0.0f;
	GLfloat lastFrame = 0.0f;
	
	// Initial rotation angle on Y axis
	GLfloat orientationY = 0.0f;
	
	// Rotation speed on Y axis
	GLfloat spin_speed = 30.0f;
	
	// Setting up our shaders
	loadShader("mirror", MIRROR);
	loadShader("skybox", SKYBOX);
	
	// Setting default binding for our cube sampler
    glUniform1i(glGetUniformLocation(programs[SKYBOX], "skybox"), 0);
	glUniform1i(glGetUniformLocation(programs[MIRROR], "skybox"), 0);
	
	// Setting constant uniform values
	for (int i = 0; i < SHADERS_NUM; i++) {
		glUniformMatrix4fv(projectionMatrixLoc[i], 1, GL_FALSE, glm::value_ptr(projection));
	}
	
	// Loading our models
	to_model cube, bunny;
	if (to_loadObj("app0:cube.obj", &cube))
		fatal_error("Cannot open app0:cube.obj");
	if (to_loadObj("app0:bunny.obj", &bunny))
		fatal_error("Cannot open app0:bunny.obj");
	
	// Our skybox faces texture filenames
	char *skybox_fnames[] = {
		"app0:skybox/right.jpg",
		"app0:skybox/left.jpg",
		"app0:skybox/top.jpg",
		"app0:skybox/bottom.jpg",
		"app0:skybox/front.jpg",
		"app0:skybox/back.jpg"
	};
	
	// We generate a cube texture for our skybox
	GLuint skybox_tex;
	glGenTextures(1, &skybox_tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tex);
	
	// Loading all skybox faces
	for (int i = 0; i < 6; i++) {
		int width, height;
		void *img = stbi_load(skybox_fnames[i], &width, &height, NULL, 3);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
		free(img);
	}
	
	// Setting up camera
	update_camera();
	
	// Main loop
	for (;;){
		// Reading inputs
		SceCtrlData pad;
		sceCtrlPeekBufferPositive(0, &pad, 1);
		
		// Calculating delta time in seconds
		GLfloat currentFrame = (float)sceKernelGetProcessTimeWide() / 1000000.0f;
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		
		// Clearing color and depth buffers
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		// Checking for wireframe mode changes requests
		if (CHECK_BTN(SCE_CTRL_CROSS)) {
			if (wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			else
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			wireframe = !wireframe;
		}
		
		// Checking for spinning mode changes requests
		if (CHECK_BTN(SCE_CTRL_CIRCLE)) {
			spinning = !spinning;
		}
		
		// Checking for flying mode changes requests
		if (CHECK_BTN(SCE_CTRL_TRIANGLE)) {
			can_fly = !can_fly;
		}
		
		// Dealing with camera orientation changes
		GLboolean needs_camera_update = GL_FALSE;
		int rx = pad.rx - 127, ry = pad.ry - 127;
		if (rx < -ANALOGS_DEADZONE) {
			camera_orientation.x += rx * camera_sensitivity;
			needs_camera_update = GL_TRUE;
		} else if (rx > ANALOGS_DEADZONE) {
			camera_orientation.x += rx * camera_sensitivity;
			needs_camera_update = GL_TRUE;
		}
		if (ry < -ANALOGS_DEADZONE) {
			camera_orientation.y -= ry * camera_sensitivity;
			needs_camera_update = GL_TRUE;
		} else if (ry > ANALOGS_DEADZONE) {
			camera_orientation.y -= ry * camera_sensitivity;
			needs_camera_update = GL_TRUE;
		}
		
		// Dealing with movements
		int lx = pad.lx - 127, ly = pad.ly - 127;
		if (lx < -ANALOGS_DEADZONE) {
			camera_pos -= camera_right * (movement_speed * deltaTime);
		} else if (lx > ANALOGS_DEADZONE) {
			camera_pos += camera_right * (movement_speed * deltaTime);
		}
		if (ly < -ANALOGS_DEADZONE) {
			camera_pos += (can_fly ? camera_front : front) * (movement_speed * deltaTime);
		} else if (ly > ANALOGS_DEADZONE) {
			camera_pos -= (can_fly ? camera_front : front) * (movement_speed * deltaTime);
		}
		
		// Performing camera setup update if required
		if (needs_camera_update) {
			// Preventing pitch to get on the "back"
			if (camera_orientation.y < -89.0f)
				camera_orientation.y = -89.0f;
			else if (camera_orientation.y > 89.0f)
				camera_orientation.y = 89.0f;
			update_camera();
		}
		
		// Properly altering rotation angle if spinning mode is enabled
		if (spinning) {
			orientationY += deltaTime * spin_speed;
		}
		
		// Setting mirror shader
		glUseProgram(programs[MIRROR]);
		
		// View matrix: camera position, view direction, camera "up" vector
		glm::mat4 view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);
		glUniformMatrix4fv(viewMatrixLoc[MIRROR], 1, GL_FALSE, glm::value_ptr(view));
		
		// Binding skybox texture
		glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tex);
		
		// Drawing bunny
		glDepthFunc(GL_LESS);
		bunnyModelMatrix = glm::mat4(1.0f);
		bunnyModelMatrix = glm::translate(bunnyModelMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
		bunnyModelMatrix = glm::rotate(bunnyModelMatrix, glm::radians(orientationY), glm::vec3(0.0f, 1.0f, 0.0f));
		bunnyModelMatrix = glm::scale(bunnyModelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
		glUniformMatrix4fv(modelMatrixLoc[MIRROR], 1, GL_FALSE, glm::value_ptr(bunnyModelMatrix));
		glUniformMatrix4fv(inverseModelMatrixLoc[MIRROR], 1, GL_FALSE, glm::value_ptr(glm::inverse(bunnyModelMatrix)));
		glUniform3fv(cameraPosLoc[MIRROR], 1, glm::value_ptr(camera_pos));
		drawModel(&bunny, GL_FALSE);
		
		// Setting skybox shader
		glUseProgram(programs[SKYBOX]);
		
		// Drawing skybox
		glDepthFunc(GL_LEQUAL);
		view = glm::mat4(glm::mat3(view)); // remove translation from the view matrix so that moving around the scene won't move the skybox
		glUniformMatrix4fv(viewMatrixLoc[SKYBOX], 1, GL_FALSE, glm::value_ptr(view));
		drawModel(&cube, GL_TRUE);
		
		// Performing buffer swap
		vglSwapBuffers(GL_FALSE);
		old_buttons = pad.buttons;
	}
	
	// Terminating graphics device
	vglEnd();
}