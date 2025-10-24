#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/animator.h>
#include <learnopengl/model_animation.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
bool checkCollision(glm::vec3 pos1, glm::vec3 size1, glm::vec3 pos2, glm::vec3 size2);

// settings
const unsigned int SCR_WIDTH = 900;
const unsigned int SCR_HEIGHT = 800;

// camera
Camera camera(glm::vec3(0.0f, 2.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float playerYaw = 0.0f;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// NEW: Game objects and state
glm::vec3 playerPos = glm::vec3(0.0f, 0.1f, 0.0f);
glm::vec3 playerSize = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 oldPlayerPos = playerPos; // For collision resolution
float playerSpeed = 5.0f;
bool isMoving = false;

const int NUM_ITEMS = 5;

glm::vec3 groundPos = glm::vec3(0.0f, -0.05f, 0.0f);
glm::vec3 groundSize = glm::vec3(50.0f, 0.1f, 50.0f);

glm::vec3 itemSize = glm::vec3(0.5f, 0.5f, 0.5f);
glm::vec3 itemPositions[NUM_ITEMS] = {
    glm::vec3(-3.0f, 0.7f, -3.0f),
    glm::vec3(3.0f, 0.7f, -5.0f),
    glm::vec3(-4.0f, 0.7f,  6.0f),
    glm::vec3(5.0f, 0.7f,  8.0f),
    glm::vec3(0.0f, 0.7f, -8.0f)
};
// NEW: Array to track which items are collected
bool itemCollected[NUM_ITEMS] = { false, false, false, false, false };

glm::vec3 doorPos = glm::vec3(5.0f, 1.0f, 0.0f);
glm::vec3 doorSize = glm::vec3(2.0f, 4.0f, 0.2f);

int itemsCollected = 0;
const int totalItems = NUM_ITEMS;

float lastDoorMessageTime = 0.0f;
const float DOOR_MESSAGE_COOLDOWN = 1.5f;

#define BASE_PATH "D:/MyWork/University/CEDT/2110582-Elec/68-GAME_EN/LearnOpenGL-master/src/8.guest/2020/skeletal_animation/"

unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);
    
    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    
    // build and compile shaders
    // -------------------------
    Shader ourShader("anim_model.vs", "anim_model.fs");
    Shader simpleShader(BASE_PATH "simple.vs", BASE_PATH "simple.fs");
    Shader objShader(BASE_PATH "obj.vs", BASE_PATH "obj.fs");

    // NEW: Add the skybox shader
    // Make sure these shader files are at the correct path!
    Shader skyboxShader(BASE_PATH "6.1.skybox.vs", BASE_PATH "6.1.skybox.fs");
    
    // load models
    // -----------
    Model ourModel(FileSystem::getPath("resources/objects/remy/remy.dae"));
    Animation danceAnimation(FileSystem::getPath("resources/objects/remy/remy_walking.dae"), &ourModel);
    Animator animator(&danceAnimation);

    Model doorModel(FileSystem::getPath("resources/objects/door/door.obj"));

    Model gearModel(FileSystem::getPath("resources/objects/gear/gear.obj"));

    Shader textureShader(BASE_PATH "texture.vs", BASE_PATH "texture.fs");

    // draw in wireframe
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    float cubeVertices[] = {
        // positions
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,

        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f
    };

    // (Add this with your other global variables)
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    float groundVertices[] = {
        // positions          // texture Coords (tiled)
         50.0f, -0.05f,  50.0f,  50.0f, 0.0f,
        -50.0f, -0.05f,  50.0f,  0.0f, 0.0f,
        -50.0f, -0.05f, -50.0f,  0.0f, 50.0f,

         50.0f, -0.05f,  50.0f,  50.0f, 0.0f,
        -50.0f, -0.05f, -50.0f,  0.0f, 50.0f,
         50.0f, -0.05f, -50.0f,  50.0f, 50.0f
    };

    // NEW: Create VAO/VBO for the cube
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glBindVertexArray(cubeVAO);
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // NEW: Ground VAO
    unsigned int groundVAO, groundVBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), &groundVertices, GL_STATIC_DRAW);
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    // Texture coord attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // NEW: Setup skybox VAO
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // NEW: Load cubemap textures
    // WARNING: You must have a "skybox" folder in your "resources"
    // with the correct 6 images!
    vector<std::string> faces
    {
        FileSystem::getPath("resources/textures/skybox/right.jpg"),
        FileSystem::getPath("resources/textures/skybox/left.jpg"),
        FileSystem::getPath("resources/textures/skybox/top.jpg"),
        FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
        FileSystem::getPath("resources/textures/skybox/front.jpg"),
        FileSystem::getPath("resources/textures/skybox/back.jpg")
    };
    stbi_set_flip_vertically_on_load(false);
    unsigned int cubemapTexture = loadCubemap(faces);
    stbi_set_flip_vertically_on_load(true);

    // NEW: Set the skybox shader uniform once
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    // NEW: Load ground texture
    // Make sure you have this texture file!
    unsigned int groundTexture;
    glGenTextures(1, &groundTexture);
    glBindTexture(GL_TEXTURE_2D, groundTexture);
    // Set texture wrapping to GL_REPEAT
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Set texture filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Load texture
    int width, height, nrChannels;
    // Make sure stbi_set_flip_vertically_on_load(true) is still active before this!
    unsigned char* data = stbi_load(FileSystem::getPath("resources/textures/ground.jpg").c_str(), &width, &height, &nrChannels, 0);
    if (data)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture: ground.jpg" << std::endl;
    }
    stbi_image_free(data);

    // Set texture samplers (only need to do this once)
    textureShader.use();
    textureShader.setInt("ourTexture", 0);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    // NEW: Add Welcome Message
    std::cout << "=============================================" << std::endl;
    std::cout << "      WELCOME TO THE GEAR COLLECTOR!         " << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "Objective: Collect all " << totalItems << " spinning gears." << std::endl;
    std::cout << "           Then, go to the red door to win!" << std::endl;
    std::cout << "\nControls:" << std::endl;
    std::cout << "  W, A, S, D - Move the character" << std::endl;
    std::cout << "  Mouse      - Look around" << std::endl;
    std::cout << "  ESC        - Quit the game" << std::endl;
    std::cout << "\nGood luck!" << std::endl;
    std::cout << "---------------------------------------------" << std::endl;
    
    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);
        animator.UpdateAnimation(deltaTime);

		// ----------------------------------------
        for (int i = 0; i < NUM_ITEMS; i++)
        {
            // Check if this item is not collected AND we are colliding with it
            if (!itemCollected[i] && checkCollision(playerPos, playerSize, itemPositions[i], itemSize))
            {
                itemCollected[i] = true; // Mark it as collected
                itemsCollected++;

                // NEW: Improved message
                std::cout << "Gear collected! You now have " << itemsCollected << " out of " << totalItems << " gears." << std::endl;

                if (itemsCollected == totalItems)
                {
                    std::cout << "That's all of them! Now, find the door (it should be cyan)!" << std::endl;
                }
            }
        }
        // ----------------------------------------
        if (checkCollision(playerPos, playerSize, doorPos, doorSize))
        {
            if (itemsCollected == totalItems)
            {
                // NEW: Congratulations message
                std::cout << "\n---------------------------------------------------" << std::endl;
                std::cout << "           CONGRATULATIONS! YOU WIN!" << std::endl;
                std::cout << " You collected all " << totalItems << " gears and reached the door!" << std::endl;
                std::cout << "              Thanks for playing!" << std::endl;
                std::cout << "===================================================" << std::endl;
                glfwSetWindowShouldClose(window, true);
            }
            else
            {
                // Collision resolution: push player back to old position
                playerPos = oldPlayerPos;

                // MODIFIED: Check if the cooldown has passed
                if (currentFrame - lastDoorMessageTime > DOOR_MESSAGE_COOLDOWN)
                {
                    int remaining = totalItems - itemsCollected;
                    std::cout << "The door is locked! You still need to find " << remaining << " more gear(s)." << std::endl;

                    // Reset the timer
                    lastDoorMessageTime = currentFrame;
                }
            }
        }
        // ----------------------------------------
        // NEW: Add "Fall off" Game Over Check
        // We only run this check if the game isn't already set to close (i.e., you haven't won)
        if (!glfwWindowShouldClose(window))
        {
            // Define the boundaries based on your groundVertices
            const float boundaryX = 50.0f;
            const float boundaryZ = 50.0f;
            const float fallLimitY = -10.0f; // How far they can fall before dying

            // Check if player's X or Z is outside the boundaries, or if they fell too low
            if (playerPos.x > boundaryX || playerPos.x < -boundaryX ||
                playerPos.z > boundaryZ || playerPos.z < -boundaryZ ||
                playerPos.y < fallLimitY)
            {
                std::cout << "\n---------------------------------------------" << std::endl;
                std::cout << "                 GAME OVER                 " << std::endl;
                std::cout << "          You fell off the world!          " << std::endl;
                std::cout << "---------------------------------------------" << std::endl;

                // Close the game
                glfwSetWindowShouldClose(window, true);
            }
        }
        // ----------------------------------------
        // 
        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // don't forget to enable shader before setting uniforms
        ourShader.use();

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        // Third-person camera logic
        float camDistance = 7.0f;
        glm::vec3 cameraPos = playerPos - camera.Front * camDistance;
        cameraPos.y += 2.0f; // Lift camera up a bit
        glm::mat4 view = glm::lookAt(cameraPos, playerPos, camera.Up);
        
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        auto transforms = animator.GetFinalBoneMatrices();
        for (int i = 0; i < transforms.size(); ++i)
            ourShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);

        // render the loaded model
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, playerPos); // translate it down so it's at the center of the scene
        model = glm::scale(model, glm::vec3(.3f, .3f, .3f)); // it's a bit too big for our scene, so scale it down
        ourShader.setMat4("model", model);
        ourModel.Draw(ourShader);

        // Draw Ground
        textureShader.use();
        textureShader.setMat4("projection", projection);
        textureShader.setMat4("view", view);

        // Set model matrix (just identity, since vertices are already in world space)
        model = glm::mat4(1.0f);
        textureShader.setMat4("model", model);

        // Bind the ground texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, groundTexture);

        // Draw the ground
        glBindVertexArray(groundVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        simpleShader.use();
        simpleShader.setMat4("projection", projection);
        simpleShader.setMat4("view", view);

        glBindVertexArray(cubeVAO);

        // Door
        if (itemsCollected == totalItems)
            simpleShader.setVec3("ourColor", 0.2f, 1.0f, 1.0f); // Cyan
        else
            simpleShader.setVec3("ourColor", 1.0f, 0.2f, 0.2f); // Red

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(5.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(2.0f, 4.0f, 0.2f));

        simpleShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        objShader.use(); // MODIFIED: Use objShader
        objShader.setMat4("projection", projection);
        objShader.setMat4("view", view);

        // Item
        for (int i = 0; i < NUM_ITEMS; i++)
        {
            // Only draw the item if it has not been collected
            if (!itemCollected[i])
            {
                model = glm::mat4(1.0f);
                model = glm::translate(model, itemPositions[i]);

                // Rotate it to stand up (like the door)
                model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

                // Add a cool spinning animation!
                model = glm::rotate(model, (float)glfwGetTime()*5, glm::vec3(0.0f, 0.0f, 1.0f));

                // Adjust this scale if the gears are too big or small
                model = glm::scale(model, glm::vec3(0.03f, 0.03f, 0.03f));

                objShader.setMat4("model", model);
                gearModel.Draw(objShader);
            }
        }

        objShader.use();

        // view/projection transformations
        objShader.setMat4("projection", projection);
        objShader.setMat4("view", view);

        // render the loaded model
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(5.0f, -0.2f, 0.0f)); // translate it down so it's at the center of the scene
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
        objShader.setMat4("model", model);
        doorModel.Draw(objShader);
        
        // NEW: Draw skybox as last
        // ------------------------
        glDepthFunc(GL_LEQUAL);  // Change depth function so skybox passes when values are equal to depth buffer's content
        skyboxShader.use();

        // Remove translation from the view matrix
        glm::mat4 skyboxView = glm::mat4(glm::mat3(view));

        skyboxShader.setMat4("view", skyboxView);
        skyboxShader.setMat4("projection", projection);

        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // Set depth function back to default

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------

    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float velocity = playerSpeed * deltaTime;
    oldPlayerPos = playerPos;

    glm::vec3 forward = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
    glm::vec3 right = camera.Right;

    glm::vec3 moveDir = glm::vec3(0.0f, 0.0f, 0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        moveDir += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        moveDir -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        moveDir -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        moveDir += right;

    if (glm::length(moveDir) > 0.0f)
    {
        moveDir = glm::normalize(moveDir);
        playerPos += moveDir * velocity;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}

bool checkCollision(glm::vec3 pos1, glm::vec3 size1, glm::vec3 pos2, glm::vec3 size2)
{
    // Get half-sizes
    glm::vec3 halfSize1 = size1 * 0.5f;
    glm::vec3 halfSize2 = size2 * 0.5f;

    // Get min/max coordinates for box 1
    glm::vec3 min1 = pos1 - halfSize1;
    glm::vec3 max1 = pos1 + halfSize1;

    // Get min/max coordinates for box 2
    glm::vec3 min2 = pos2 - halfSize2;
    glm::vec3 max2 = pos2 + halfSize2;

    // Check for overlap on all three axes
    bool collisionX = max1.x >= min2.x && max2.x >= min1.x;
    bool collisionY = max1.y >= min2.y && max2.y >= min1.y;
    bool collisionZ = max1.z >= min2.z && max2.z >= min1.z;

    // Collision only if there is overlap on all axes
    return collisionX && collisionY && collisionZ;
}