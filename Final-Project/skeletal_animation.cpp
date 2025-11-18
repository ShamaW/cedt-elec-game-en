#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glm/gtc/constants.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/model_animation.h>
#include <learnopengl/animator.h>
#include <stb_image.h>

#include <learnopengl/shader_m.h> 

#include <iostream>
#include <vector>
#include <string>

#include <irrklang/irrKlang.h>

#pragma comment(lib, "irrKlang.lib")

using namespace irrklang;

#define BASE_PATH ""

// --- Function Prototypes ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window);

// --- Game Settings ---
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// --- Character & Camera ---
glm::vec3 characterPos = glm::vec3(0.0f, 1.0f, 32.0f); // Character's center
glm::vec3 characterSpawnPos = glm::vec3(0.0f, 1.0f, 32.0f); // Spawn point
glm::vec3 playerSize = glm::vec3(0.5f, 2.0f, 0.5f); // Collision box
float characterYaw = -90.0f; // Start facing down the -Z axis
float characterSpeed = 5.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool isPlayerAlive = true; // Game state
int playerLives = 3; // Player lives 

// --- Animation State ---
bool isMoving = false;
bool isRunning = false;

// --- Jump Physics ---
float characterVelocityY = 0.0f;
const float GRAVITY = -18.0f;
const float JUMP_POWER = 8.0f;
bool canJump = true;

// --- Trampoline ---
const float TRAMPOLINE_POWER = 40.0f; // High upward velocity
glm::vec3 trampolinePos = glm::vec3(0.0f, 0.0f, -95.0f);
glm::vec3 trampolineJumpPos = glm::vec3(0.0f, 0.5f, -64.0f);
glm::vec3 trampolineSize = glm::vec3(4.0f, 1.0f, 4.0f);

// --- Parry Logic ---
float parryTimer = 0.0f;
const float parryDuration = 0.25f;
bool parryKeyPressed = false; // Prevents holding 'E' to auto-parry

// --- Struct Definitions ---

// Holds all data for a single small enemy
struct Enemy {
	glm::vec3 position;
	bool isAlive = true;
	bool isTriggered = false;
	float shootTimer = 0.0f;
	Animator animator;

	Enemy(glm::vec3 pos, Animation* idleAnim)
		: position(pos), animator(idleAnim) {
	}
};

// Holds all data for a single projectile
struct Projectile {
	glm::vec3 position;
	glm::vec3 velocity;
	bool isParried = false;
	int enemyIndex; // Which enemy fired it? (-1 if boss)
	bool isBossProjectile = false;
};

// Holds all data for the final boss
struct Boss {
	glm::vec3 position = glm::vec3(0.0f, 38.0f, -90.0f);
	glm::vec3 size = glm::vec3(1.0f, 1.0f, 1.0f);
	bool isAlive = true;
	float health = 10.0f; // Boss health
	const float maxHealth = 10.0f;
	float shootTimer = 0.0f;
	float hitTimer = 0.5f; // Cooldown for player hitting boss
	int hitCombo = 0;      // Tracks consecutive hits
	float comboTimer = 0.0f; // Time left to continue the combo
};

// --- Global Game State ---
std::vector<Enemy> enemies;
std::vector<Projectile> projectiles;
Boss boss;

int enemiesDefeatedCount = 0; // How many small enemies are dead
bool bossBattleActive = false; // Is the player on the runway or in the boss arena?
bool bossFightStarted = false;
const float BOSS_TRIGGER_Z = -80.0f;

// --- World Boundaries ---
const float RUNWAY_MIN_X = -5.5f;
const float RUNWAY_MAX_X = 5.5f;
const float RUNWAY_MIN_Z = -65.0f; // Slightly past trigger
const float RUNWAY_MAX_Z = 35.0f;

const float ARENA_MIN_X = -20.0f;
const float ARENA_MAX_X = 20.0f;
const float ARENA_MIN_Z = -100.0f;
const float ARENA_MAX_Z = -60.0f; // Connects to runway

// --- Gameplay Constants ---
const float shootCooldown = 2.0f;
const float bossShootCooldown = 2.0f;
const float projectileSpeed = 15.0f;
const float bossComboWindow = 5.0f; // 5 seconds to continue combo

// --- Timing ---
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Audio Globals 
ISoundEngine* soundEngine = nullptr;
ISound* walkingSound = nullptr;
const float musicVolume = 0.3f;
const float walkingVolume = 0.4f;
const float sfxVolume = 0.6f;

bool checkCollisionAABB(glm::vec3 center1, glm::vec3 size1, glm::vec3 center2, glm::vec3 size2) {
	// Calculate min and max coordinates for box 1
	glm::vec3 min1 = center1 - size1 / 2.0f;
	glm::vec3 max1 = center1 + size1 / 2.0f;
	// Calculate min and max coordinates for box 2
	glm::vec3 min2 = center2 - size2 / 2.0f;
	glm::vec3 max2 = center2 + size2 / 2.0f;

	// Check for overlap on all three axes
	bool overlapX = (min1.x <= max2.x) && (max1.x >= min2.x);
	bool overlapY = (min1.y <= max2.y) && (max1.y >= min2.y);
	bool overlapZ = (min1.z <= max2.z) && (max1.z >= min2.z);

	// Collision only occurs if all three axes overlap
	return overlapX && overlapY && overlapZ;
}

unsigned int loadTexture(char const* path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum format;
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// Set texture wrapping to GL_REPEAT
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// ------------------------------

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
		std::cout << "Loaded texture at path: " << path << std::endl;
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}

	return textureID;
}


int main()
{
	// --- GLFW & GLAD Initialization ---
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "SAMYAN FIGHTTOWN", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// --- NEW: Initialize Audio Engine ---
	soundEngine = createIrrKlangDevice();
	if (!soundEngine) {
		std::cout << "Could not start irrKlang sound engine." << std::endl;
		return -1;
	}

	stbi_set_flip_vertically_on_load(true);
	glEnable(GL_DEPTH_TEST);

	// --- Shader Compilation ---
	Shader simpleCubeShader(BASE_PATH "cube.vs", BASE_PATH "cube.fs");
	Shader animShader(BASE_PATH "anim_model.vs", BASE_PATH "anim_model.fs"); // For 3D objects
	Shader hudShader(BASE_PATH "hud.vs", BASE_PATH "hud.fs"); // For 2D health bar
	Shader floorShader(BASE_PATH "floor.vs", BASE_PATH "floor.fs");
	Shader tunnelShader(BASE_PATH "model_static.vs", BASE_PATH "model_static.fs");
	Shader spriteShader(BASE_PATH "sprite.vs", BASE_PATH "sprite.fs");

	// --- Load Models & Animations ---
	std::string modelBasePath = std::string(BASE_PATH) + "objects/remy/";
	std::string remyPath = modelBasePath + "remy.dae";
	std::string idlePath = modelBasePath + "remy_idle.dae";
	std::string walkPath = modelBasePath + "remy_walking.dae";
	std::string runPath = modelBasePath + "remy_running.dae";
	std::string surprisePath = modelBasePath + "remy_surprised.dae";
	std::string jumpPath = modelBasePath + "remy_jump.dae";
	std::string parryPath = modelBasePath + "remy_parry.dae";
	std::string deathPath = modelBasePath + "remy_death.dae";

	Model ourModel(remyPath);
	Animation idleAnimation(idlePath, &ourModel);
	Animation walkingAnimation(walkPath, &ourModel);
	Animation runningAnimation(runPath, &ourModel);
	Animation surprisedAnimation(surprisePath, &ourModel);
	Animation jumpAnimation(jumpPath, &ourModel);
	Animation parryAnimation(parryPath, &ourModel);
	Animation deathAnimation(deathPath, &ourModel);

	Animator animator(&idleAnimation);
	Animation* pCurrentAnimation = &idleAnimation;

	std::string texturePath = std::string(BASE_PATH) + "objects/tile_basement.png";
	unsigned int floorTexture = loadTexture(texturePath.c_str());
	
	// Load Tunnel Model
	std::string tunnelPath = std::string(BASE_PATH) + "objects/tunnel/tunnel.obj";
	Model tunnelModel(tunnelPath);

	// Load Hall Model
	std::string hallPath = std::string(BASE_PATH) + "objects/hall/hall.obj";
	Model hallModel(hallPath);

	// Load Fences and Poles
	std::string fencesPath = std::string(BASE_PATH) + "objects/hall/fences/fences.obj";
	std::string polesPath = std::string(BASE_PATH) + "objects/hall/fences/poles.obj";
	Model fenceModel(fencesPath);
	Model poleModel(polesPath);

	// Load the Trampoline Model
	std::string trampolinePath = std::string(BASE_PATH) + "objects/trampoline/trampoline.obj";
	Model trampolineModel(trampolinePath);

	// Load the Trampoline Room Model
	std::string roomPath = std::string(BASE_PATH) + "objects/trampoline_room/trampoline_room.obj";
	Model trampolineRoomModel(roomPath);

	// Load the Bullet Model
	std::string bulletPath = std::string(BASE_PATH) + "objects/bullet/bullet.obj";
	Model bulletModel(bulletPath);

	// --- NEW: Load Boss Models & Animations ---
	std::string bossModelBasePath = std::string(BASE_PATH) + "objects/boss/";

	std::string bossAttackPath = bossModelBasePath + "boss_attack.dae";
	Model bossAttackModel(bossAttackPath);
	Animation bossAttackAnimation(bossAttackPath, &bossAttackModel);
	Animator bossAnimator(&bossAttackAnimation);

	// --- NEW: Load Enemy Model & Animation ---
	std::string enemyModelBasePath = std::string(BASE_PATH) + "objects/enemy/";
	std::string enemyPath = enemyModelBasePath + "enemy.dae";
	Model enemyModel(enemyPath);
	Animation enemyIdleAnimation(enemyPath, &enemyModel);

	// --- Vertex Data & Buffers (VAOs/VBOs) ---

	// Vertex data for a simple cube
	float cubeVertices[] = {
		-0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
		-0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,
		-0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
	};
	// Vertex data for the runway plane
	float planeVertices[] = {
		// positions          // texture coords
		 5.5f, 0.0f,  35.0f,  0.3f, 8.5f,
		-5.5f, 0.0f, -60.0f,   0.0f, 0.0f,
		-5.5f, 0.0f,  35.0f,   0.0f, 8.5f,

		 5.5f, 0.0f,  35.0f,  0.3f, 8.5f,
		 5.5f, 0.0f, -60.0f,  0.3f, 0.0f,
		-5.5f, 0.0f, -60.0f,   0.0f, 0.0f
	};

	// Vertex data for the boss arena plane (set slightly lower to prevent Z-fighting)
	float arenaVertices[] = {
		// positions          // texture coords
		 20.0f, 20.0f, -60.0f,  40.0f, 30.0f,
		-20.0f, 20.0f, -90.0f,   0.0f, 0.0f,
		-20.0f, 20.0f, -60.0f,   0.0f, 30.0f,

		 20.0f, 20.0f, -60.0f,  40.0f, 30.0f,
		 20.0f, 20.0f, -90.0f,  40.0f, 0.0f,
		-20.0f, 20.0f, -90.0f,   0.0f, 0.0f
	};

	// Vertex data for the 2D HUD (a simple 2D quad)
	float hudVertices[] = {
		0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f
	};

	float spriteVertices[] = {
		// positions        // texture coords
		0.5f, 0.5f, 0.0f,   1.0f, 1.0f, // top right
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, // bottom left
		-0.5f, 0.5f, 0.0f,  0.0f, 1.0f, // top left

		0.5f, 0.5f, 0.0f,   1.0f, 1.0f, // top right
		0.5f, -0.5f, 0.0f,  1.0f, 0.0f, // bottom right
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f  // bottom left
	};

	// --- Cube VAO/VBO ---
	unsigned int cubeVAO, cubeVBO;
	glGenVertexArrays(1, &cubeVAO);
	glGenBuffers(1, &cubeVBO);
	glBindVertexArray(cubeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// --- Runway Plane VAO/VBO ---
	unsigned int planeVAO, planeVBO;
	glGenVertexArrays(1, &planeVAO);
	glGenBuffers(1, &planeVBO);
	glBindVertexArray(planeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

	// position attribute (layout = 0)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// texture coord attribute (layout = 1)
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// --- Boss Arena VAO/VBO ---
	unsigned int arenaVAO, arenaVBO;
	glGenVertexArrays(1, &arenaVAO);
	glGenBuffers(1, &arenaVBO);
	glBindVertexArray(arenaVAO);
	glBindBuffer(GL_ARRAY_BUFFER, arenaVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(arenaVertices), arenaVertices, GL_STATIC_DRAW);
	
	// position attribute (layout = 0)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	
	// texture coord attribute (layout = 1)
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// --- HUD VAO/VBO ---
	unsigned int hudVAO, hudVBO;
	glGenVertexArrays(1, &hudVAO);
	glGenBuffers(1, &hudVBO);
	glBindVertexArray(hudVAO);
	glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(hudVertices), hudVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	// --- NEW: Sprite Quad VAO/VBO ---
	unsigned int spriteVAO, spriteVBO;
	glGenVertexArrays(1, &spriteVAO);
	glGenBuffers(1, &spriteVBO);
	glBindVertexArray(spriteVAO);
	glBindBuffer(GL_ARRAY_BUFFER, spriteVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(spriteVertices), spriteVertices, GL_STATIC_DRAW);
	
	// position attribute (layout = 0)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	
	// texture coord attribute (layout = 1)
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	
	// --- NEW: Load Poster Texture ---
	// Make sure you have a "poster.png" (or similar) at this path
	std::string posterPath = std::string(BASE_PATH) + "objects/poster.png";
	unsigned int posterTexture = loadTexture(posterPath.c_str());
	

	// --- Game Object Initialization ---
	enemies.push_back(Enemy(glm::vec3(5.0f, 1.0f, 10.0f), &enemyIdleAnimation));
	enemies.push_back(Enemy(glm::vec3(-5.0f, 1.0f, -20.0f), &enemyIdleAnimation));
	enemies.push_back(Enemy(glm::vec3(5.0f, 1.0f, -40.0f), &enemyIdleAnimation));

	// --- NEW: Welcome Message ---
	std::cout << "============================================================================" << std::endl;
	std::cout << "                     WELCOME TO THE SAMYAN FIGHTTOWN!                       " << std::endl;
	std::cout << "============================================================================" << std::endl;
	std::cout << "Objective: You are assigned to fight with the 'Great of Carbon' of the cave!" << std::endl;
	std::cout << "           Let's see if you can do it or not!" << std::endl;
	std::cout << "\nControls:" << std::endl;
	std::cout << "  W, A, S, D - Move                |  For the boss you have to JUMP and HIT " << std::endl;
	std::cout << "  Shift      - Run                 |  the boss. That way you can kill Him!  " << std::endl;
	std::cout << "  E          - Parry (Runway Only) |  Remember, you can't parry at the      " << std::endl;
	std::cout << "  Space      - Jump                |  boss' bullet and your life is         " << std::endl;
	std::cout << "  Mouse      - Look around         |  reduce to 1 when with boss            " << std::endl;
	std::cout << "  ESC        - Quit the game       |  Don't forget to JUMP!!!               " << std::endl;
	std::cout << "\nYou have " << playerLives << " lives. Good luck." << std::endl;
	std::cout << "----------------------------------------------------------------------------" << std::endl;
	std::cout << "============================================================================" << std::endl;
	std::cout << "Music:         Games by Pold https://soundcloud.com/pold-music" << std::endl;
	std::cout << "License:       CC BY-SA 3.0" << std::endl;
	std::cout << "Free Download: https://audiolibrary.com.co/pold/games" << std::endl;
	std::cout << "Music promoted by Audio Library: https://youtu.be/SyFPLhALc5w" << std::endl;	
	std::cout << "----------------------------------------------------------------------------" << std::endl;
	std::cout << "Sound Effect by Vicki Hamilton from Pixabay" << std::endl;
	std::cout << "Sound Effect by Alphix from Pixabay" << std::endl;
	std::cout << "Sound Effect by Sophia_C from freesound.org -- License: Attribution 4.0" << std::endl;
	std::cout << "Sound Effect by LilMati from freesound.org -- License: Creative Commons 0" << std::endl;
	std::cout << "Sound Effect by nomiqbomi from freesound.org -- License: Creative Commons 0" << std::endl;
	std::cout << "Sound Effect by GameAudio from freesound.org -- License: Creative Commons 0" << std::endl;
	std::cout << "Sound Effect by MadPanCake from freesound.org -- License: Creative Commons 0" << std::endl;
	std::cout << "============================================================================" << std::endl;

	// --- Start Audio ---
	const std::string audioPath = std::string(BASE_PATH) + "audio/";
	ISound* bgMusic = soundEngine->play2D((audioPath + "music.mp3").c_str(), true, false, true);
	if (bgMusic) bgMusic->setVolume(musicVolume); // Set background music volume

	// Pre-load walking sound, start it paused
	walkingSound = soundEngine->play2D((audioPath + "walking.mp3").c_str(), true, true, true);
	if (walkingSound) walkingSound->setVolume(walkingVolume); // Set walking sound volume
	
	// Death Animation State
	float deathAnimationTimer = 0.0f;
	bool isDeathAnimationFinished = false;

	// --- Main Game Loop ---
	while (!glfwWindowShouldClose(window))
	{
		// --- Timing & Frame Logic ---
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// Store position before movement to check against boundaries
		glm::vec3 lastSafePos = characterPos;
		// Define object sizes for collision
		glm::vec3 enemySize = glm::vec3(1.0f, 2.0f, 1.0f);

		// --- Player's "ground" height changes ---
		float playerHeightOnGround = bossBattleActive ? 21.0f : 1.0f; // 1.0 for runway, 21.0 for arena

		// --- Input ---
		processInput(window);

		// --- Physics Update (Gravity & Jump) ---
		if (!canJump) { // If player is in the air
			characterVelocityY += GRAVITY * deltaTime; // Apply gravity
			characterPos.y += characterVelocityY * deltaTime; // Update vertical position

			// --- MODIFIED: Dynamic landing height ---
			if (characterPos.y <= playerHeightOnGround) {
				characterPos.y = playerHeightOnGround; // Land on the correct surface
				characterVelocityY = 0.0f;
				canJump = true;
			}
		}

		// --- Boundary Collision (Invisible Walls) ---
		float playerHalfWidth = playerSize.x / 2.0f;
		float playerHalfDepth = playerSize.z / 2.0f;

		if (!bossBattleActive)
		{
			// --- On the Runway ---
			if (characterPos.x - playerHalfWidth < RUNWAY_MIN_X || characterPos.x + playerHalfWidth > RUNWAY_MAX_X)
				characterPos.x = lastSafePos.x; // Revert X
			if (characterPos.z - playerHalfDepth < RUNWAY_MIN_Z || characterPos.z + playerHalfDepth > RUNWAY_MAX_Z)
				characterPos.z = lastSafePos.z; // Revert Z
		}
		else
		{
			// --- On the Boss Platform (bossBattleActive == true) ---

			// --- Area 1: OUTER "global" walls ---
			// This keeps the player from falling off the entire platform
			if (characterPos.x - playerHalfWidth < ARENA_MIN_X || characterPos.x + playerHalfWidth > ARENA_MAX_X)
				characterPos.x = lastSafePos.x; // Revert X
			if (characterPos.z - playerHalfDepth < ARENA_MIN_Z || characterPos.z + playerHalfDepth > ARENA_MAX_Z)
				characterPos.z = lastSafePos.z; // Revert Z


			// --- Area 2: INNER "one-way" wall (CORRECTED) ---
			// We check if the fight has started AND if the player's CENTER is trying
			// to move back past the trigger line.

			if (bossFightStarted && (characterPos.z > BOSS_TRIGGER_Z))
			{
				// The fight is on, and the player's center is trying to go back.
				// Revert their Z position to "lock" them in the attack zone.
				characterPos.z = lastSafePos.z;
			}
		}

		// --- Core Game Logic ---
		// Tick down active timers
		if (parryTimer > 0.0f) 
			parryTimer -= deltaTime;
		if (boss.hitTimer > 0.0f) 
			boss.hitTimer -= deltaTime;

		// --- Animation State Machine ---
		Animation* pNextAnimation = pCurrentAnimation;
		if (!isPlayerAlive)
		{
			if (!isDeathAnimationFinished)
				pNextAnimation = &deathAnimation;
		}
		else if (!boss.isAlive)
		{
			pNextAnimation = &surprisedAnimation;
		}
		else if (parryTimer > 0.0f)
		{
			pNextAnimation = &parryAnimation;
		}
		else if (!canJump)
		{
			pNextAnimation = &jumpAnimation;
		}
		else if (isMoving)
		{
			if (isRunning)
				pNextAnimation = &runningAnimation;
			else
				pNextAnimation = &walkingAnimation;
		}
		else // Not moving
		{
			pNextAnimation = &idleAnimation;
		}
		if (pCurrentAnimation != pNextAnimation)
		{
			animator.PlayAnimation(pNextAnimation);
			pCurrentAnimation = pNextAnimation;

			// Reset timer when death animation starts
			if (pCurrentAnimation == &deathAnimation)
			{
				deathAnimationTimer = 0.0f; // Start our timer
				isDeathAnimationFinished = false;
			}
		}

		// --- Animation Update Logic ---
		if (pCurrentAnimation == &deathAnimation && !isDeathAnimationFinished)
		{
			// Check if this frame's update would push the time *past* the duration
			if (deathAnimationTimer + deltaTime >= deathAnimation.GetDuration())
			{
				// Animation will finish this frame, so snap to end
				float targetTime = deathAnimation.GetDuration() * 0.999f;

				if (deathAnimationTimer < targetTime)
				{
					float timeToTarget = targetTime - deathAnimationTimer;
					animator.UpdateAnimation(timeToTarget);
				}

				isDeathAnimationFinished = true; // Set the flag to true!
			}
			else
			{
				// Animation is not finished, update normally
				animator.UpdateAnimation(deltaTime);
				deathAnimationTimer += deltaTime; // Add to our timer
			}
		}
		else if (pCurrentAnimation != &deathAnimation)
		{
			animator.UpdateAnimation(deltaTime);
		}

		// --- Game State FSM (Finite State Machine) ---
		// This block switches between "Runway" logic and "Boss" logic
		if (!bossBattleActive)
		{
			// --- RUNWAY LOGIC ---

			// Check for trampoline collision
			if (enemiesDefeatedCount == enemies.size() && // 1. All enemies must be dead
				characterVelocityY < 0.0f &&            // 2. Player must be falling
				checkCollisionAABB(characterPos, playerSize, trampolineJumpPos, trampolineSize)) // 3. Player hits trampoline
			{
				// --- TRAMPOLINE LAUNCH ---
				bossBattleActive = true;
				characterVelocityY = TRAMPOLINE_POWER; // Launch player high up
				canJump = false; // Player is in the air

				std::cout << "---------------------------------" << std::endl;
				std::cout << "TRAMPOLINE LAUNCH! BOSS BATTLE START!" << std::endl;
				std::cout << "---------------------------------" << std::endl;
				projectiles.clear(); // Clear runway projectiles
			}

			// Update Small Enemies
			for (int i = 0; i < enemies.size(); ++i)
			{
				if (enemies[i].isAlive)
				{
					enemies[i].animator.UpdateAnimation(deltaTime);
				}

				// Check if player has triggered the enemy
				if (enemies[i].isAlive && !enemies[i].isTriggered) {
					if (characterPos.z < enemies[i].position.z + 7.0f) {
						enemies[i].isTriggered = true;
						std::cout << "ENEMY " << i << " SPOTTED YOU!" << std::endl;
					}
				}

				// If triggered and alive, try to shoot
				if (isPlayerAlive && enemies[i].isAlive && enemies[i].isTriggered) {
					enemies[i].shootTimer += deltaTime;
					if (enemies[i].shootTimer >= shootCooldown) {
						enemies[i].shootTimer = 0.0f; // Reset timer
						// Create and fire a new projectile
						Projectile newProjectile;
						newProjectile.position = enemies[i].position;
						glm::vec3 direction = glm::normalize(characterPos - enemies[i].position);
						newProjectile.velocity = direction * projectileSpeed;
						newProjectile.enemyIndex = i;
						newProjectile.isBossProjectile = false;
						projectiles.push_back(newProjectile);
					}
				}
			}
		}
		else
		{
			// --- BOSS BATTLE LOGIC ---

			// --- NEW: Boss Trigger Logic ---
			// First, check if the fight has started. If not, check if we should start it.
			if (!bossFightStarted)
			{
				// Check if player has crossed the "invisible line"
				if (characterPos.z < BOSS_TRIGGER_Z)
				{
					bossFightStarted = true; // START THE FIGHT!
					std::cout << "=================================" << std::endl;
					std::cout << "      BOSS BATTLE START!         " << std::endl;

					// --- MOVED --- This logic is now here
					if (playerLives > 1) {
						std::cout << "Your extra lives vanish... It's all or nothing." << std::endl;
						playerLives = 1;
						std::cout << "You have only " << playerLives << " live. Good Luck!" << std::endl;
					}
					std::cout << "=================================" << std::endl;
				}
			}
			// --- END NEW LOGIC ---


			// --- MODIFIED: Boss Update Logic ---
			// Only run boss AI/attacks *after* the fight has been triggered
			if (bossFightStarted && isPlayerAlive && boss.isAlive)
			{
				// Update Boss Combo Timer
				if (boss.comboTimer > 0.0f)
				{
					boss.comboTimer -= deltaTime;
					if (boss.comboTimer <= 0.0f)
					{
						std::cout << "Combo reset!" << std::endl;
						boss.hitCombo = 0;
					}
				}

				// Boss Shooting
				boss.shootTimer += deltaTime;
				if (boss.shootTimer >= bossShootCooldown)
				{
					boss.shootTimer = 0.0f;
					// Create and fire a new (boss) projectile
					// Create a "shoot from" position
					glm::vec3 shootPosition = boss.position;

					// Apply the SAME compensation used for drawing
					shootPosition.y -= 13.5f;

					// Create and fire a new (boss) projectile
					Projectile newProjectile;
					// Use the new, corrected position
					newProjectile.position = shootPosition;
					// Also calculate direction from the corrected position
					glm::vec3 direction = glm::normalize(characterPos - shootPosition);
					newProjectile.velocity = direction * projectileSpeed * 1.2f; // Shoots faster

					newProjectile.enemyIndex = -1; // -1 means it's from the boss
					newProjectile.isBossProjectile = true;
					projectiles.push_back(newProjectile);
				}

				// Boss Collision and Attack Logic
				// --- START HITBOX FIX ---
				// Create a temporary hitbox position that matches the visual model
				glm::vec3 bossHitboxPos = boss.position;
				bossHitboxPos.y -= 16.5f;

				if (checkCollisionAABB(characterPos, playerSize, bossHitboxPos, boss.size))
				{
					// Use the corrected hitbox position
					float bossHeadY = bossHitboxPos.y + (boss.size.y / 2.0f);

					// 1. Check for Head Jump (Attack)
					// (All the rest of your boss collision/attack logic stays the same)
					// ...
					// Use the corrected hitbox position
					if (characterVelocityY < 0.0f && characterPos.y > bossHitboxPos.y && boss.hitTimer <= 0.0f)
					{
						// --- HEAD JUMP SUCCESS ---
						ISound* snd = soundEngine->play2D((audioPath + "attack_boss.wav").c_str(), false, false, true);
						if (snd) snd->setVolume(sfxVolume);

						boss.hitTimer = 2.0f; // Boss is invulnerable for 2s
						boss.comboTimer = bossComboWindow; // Reset combo window

						// Combo Logic
						if (boss.hitCombo == 3)
						{
							// 4th Hit: CRITICAL
							boss.health -= 2.0f;
							std::cout << "CRITICAL HIT! -2 Health! Combo reset! Health: " << boss.health << std::endl;
							boss.hitCombo = 0; // Reset combo
						}
						else
						{
							// 1st, 2nd, or 3rd Hit: NORMAL
							boss.health -= 1.0f;
							boss.hitCombo++;
							std::cout << "BOSS HIT! Combo: " << boss.hitCombo << "/3. Health: " << boss.health << std::endl;
						}

						characterVelocityY = JUMP_POWER * 1.2f; // Bounce off boss
						canJump = false; // We are in the air

						// Snap player position to top of boss's head
						characterPos.y = bossHeadY + (playerSize.y / 2.0f);

						// Check for boss death
						if (boss.health <= 0.0f)
						{
							boss.isAlive = false;
							std::cout << "=================================" << std::endl;
							std::cout << "     BOSS DEFEATED! YOU WIN!     " << std::endl;
							std::cout << "=================================" << std::endl;
							projectiles.clear(); // Clear all bullets

							// Play Victory Sound
							soundEngine->stopAllSounds();
							soundEngine->play2D((audioPath + "congrats.mp3").c_str());
						}
					}
					// 2. Check for Landing on Head (no attack)
					else if (characterVelocityY < 0.0f && characterPos.y > bossHitboxPos.y)
					{
						// Player is on head, but attack is on cooldown
						canJump = true; // Allow jumping off the boss
						characterVelocityY = 0.0f;
						characterPos.y = bossHeadY + (playerSize.y / 2.0f);
					}
					// 3. Body Collision
					else
					{
						// Collision with the side of the boss; push player back
						characterPos.x = lastSafePos.x;
						characterPos.z = lastSafePos.z;
					}
				} // End of Boss Collision Block
			} // --- End of "if (bossFightStarted...)" block
		} // End of Game Logic FSM


		// --- Projectile Update & Collision Logic (Applies to both modes) ---
		for (int i = 0; i < projectiles.size(); ++i) {
			projectiles[i].position += projectiles[i].velocity * deltaTime;

			// Determine projectile size
			glm::vec3 projectileSize = projectiles[i].isBossProjectile ? glm::vec3(0.5f, 0.5f, 0.5f) : glm::vec3(0.2f, 0.2f, 0.2f);

			// Logic for parried (reflected) projectiles
			if (projectiles[i].isParried) {
				int targetEnemyIndex = projectiles[i].enemyIndex;

				// Check collision with the enemy it's flying towards
				if (enemies[targetEnemyIndex].isAlive &&
					checkCollisionAABB(enemies[targetEnemyIndex].position, enemySize, projectiles[i].position, projectileSize))
				{
					// Play Enemy Death Sounds
					ISound* snd = soundEngine->play2D((audioPath + "enemy_death.wav").c_str(), false, false, true);
					if (snd) snd->setVolume(sfxVolume);

					enemies[targetEnemyIndex].isAlive = false;
					std::cout << "ENEMY " << targetEnemyIndex << " DEFEATED!" << std::endl;
					enemiesDefeatedCount++;
					std::cout << "Enemies remaining: " << enemies.size() - enemiesDefeatedCount << std::endl;

					// Check if all enemies are dead
					if (enemiesDefeatedCount == enemies.size()) {
						std::cout << "All enemies cleared! Proceed to the end of the cave." << std::endl;
					}

					projectiles.erase(projectiles.begin() + i); // Delete projectile
					i--;
					continue;
				}
			}
			else {
				// Logic for normal or boss projectiles (check player collision)
				if (isPlayerAlive && checkCollisionAABB(characterPos, playerSize, projectiles[i].position, projectileSize)) {

					// Check for PARRY (cannot parry boss projectiles)
					if (parryTimer > 0.0f && !projectiles[i].isBossProjectile) {
						// --- PARRY SUCCESSFUL ---
						projectiles[i].isParried = true;
						int targetEnemyIndex = projectiles[i].enemyIndex;
						// Reverse velocity back towards the enemy
						projectiles[i].velocity = glm::normalize(enemies[targetEnemyIndex].position - projectiles[i].position) * projectileSpeed * 1.5f;
						parryTimer = 0.0f; // End parry
						std::cout << "PARRY!" << std::endl;

						// Play parry sound
						ISound* snd = soundEngine->play2D((audioPath + "attack_enemy.wav").c_str(), false, false, true);
						if (snd) snd->setVolume(sfxVolume);
					}
					else {
						// --- PARRY FAILED / HIT BY BULLET ---

						if (projectiles[i].isBossProjectile) {
							// Boss projectiles are instant death
							playerLives = 0;
							isPlayerAlive = false;
							std::cout << "---------------------------------" << std::endl;
							std::cout << "       YOU DIED! Game Over.      " << std::endl;
							std::cout << "---------------------------------" << std::endl;

							// Play Player Death Sound
							soundEngine->stopAllSounds();
							soundEngine->play2D((audioPath + "player_death.mp3").c_str());
						}
						else {
							// Normal projectiles remove one life
							playerLives--;
							std::cout << "HIT! Lives remaining: " << playerLives << std::endl;

							if (playerLives > 0) {
								// Respawn at the start
								ISound* snd = soundEngine->play2D((audioPath + "player_reborn.wav").c_str(), false, false, true);
								if (snd) snd->setVolume(sfxVolume);

								characterPos = characterSpawnPos;
								characterYaw = -90.0f;
								canJump = true;
								characterVelocityY = 0.0f;
							}
							else {
								// No lives left
								isPlayerAlive = false;
								std::cout << "---------------------------------" << std::endl;
								std::cout << "       YOU DIED! Game Over.      " << std::endl;
								std::cout << "---------------------------------" << std::endl;

								// Play Player Death Sound
								soundEngine->stopAllSounds();
								ISound* snd = soundEngine->play2D((audioPath + "player_death.mp3").c_str(), false, false, true);
								if (snd) snd->setVolume(musicVolume); // Play at music volume
							}
						}

						projectiles.erase(projectiles.begin() + i); // Delete projectile
						i--;
						continue;
					}
				}
			}

			// Cleanup projectiles that fly too far or fall through the world
			if (projectiles[i].position.y < -1.0f || glm::length(projectiles[i].position - characterPos) > 100.0f) {
				projectiles.erase(projectiles.begin() + i);
				i--;
			}
		}


		// --- Render: Clear Screen ---
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// --- 3D Rendering Pass ---
		//ourShader.use();
		glEnable(GL_DEPTH_TEST); // Make sure depth test is on for 3D

		// Set up 3D perspective camera
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
		// Calculate camera's "front" vector based on player's yaw
		glm::vec3 front;
		front.x = cos(glm::radians(characterYaw));
		front.y = 0.0f;
		front.z = sin(glm::radians(characterYaw));
		front = glm::normalize(front);

		// --- Camera follows player up to arena ---
		glm::vec3 cameraPos = characterPos - (front * 7.0f) + glm::vec3(0.0f, 2.5f, 0.0f);
		glm::mat4 view = glm::lookAt(cameraPos, characterPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		// --- MODIFIED: Draw the floors with the new shader ---
		floorShader.use();
		floorShader.setMat4("projection", projection);
		floorShader.setMat4("view", view);

		// Bind the floor texture to texture unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		// Tell the shader to use texture unit 0
		floorShader.setInt("texSampler", 0);

		// Draw the runway
		glBindVertexArray(planeVAO);
		glm::mat4 model = glm::mat4(1.0f);
		floorShader.setMat4("model", model);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Draw Arena (only if boss battle started)
		// --- NEW: Draw the Boss Hall ---
		if (bossBattleActive) {
			tunnelShader.use();
			tunnelShader.setMat4("projection", projection);
			tunnelShader.setMat4("view", view);

			model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(0.0f, 20.0f, -65.0f));
			model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::rotate(model, glm::radians(-180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			model = glm::scale(model, glm::vec3(2.0f));

			tunnelShader.setMat4("model", model);
			hallModel.Draw(tunnelShader); // This draws the hall model

			// Draw the Fences
			model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(0.0f, 20.0f, -65.0f));
			model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::rotate(model, glm::radians(-180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			model = glm::scale(model, glm::vec3(2.0f));
			tunnelShader.setMat4("model", model);
			fenceModel.Draw(tunnelShader); // This draws the fences

			// Draw the Poles
			model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(0.0f, 20.0f, -65.0f));
			model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::rotate(model, glm::radians(-180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			model = glm::scale(model, glm::vec3(2.0f));
			tunnelShader.setMat4("model", model);
			poleModel.Draw(tunnelShader);
		}

		// --- Draw the Tunnel ---
		tunnelShader.use();
		tunnelShader.setMat4("projection", projection);
		tunnelShader.setMat4("view", view);

		// Apply transformations to the tunnel (position, rotate, scale)
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 35.1f));
		model = glm::scale(model, glm::vec3(3.8f));

		tunnelShader.setMat4("model", model);
		tunnelModel.Draw(tunnelShader); // This draws the tunnel

		// --- NEW: Draw Poster Images ---
		// Enable blending for transparency
		spriteShader.use();
		spriteShader.setInt("spriteTexture", 0);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		spriteShader.use();
		spriteShader.setMat4("projection", projection);
		spriteShader.setMat4("view", view);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, posterTexture);
		glBindVertexArray(spriteVAO);
		
		// Draw Left Image
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(-5.4f, 2.0f, 30.0f)); // Position: Left wall (slightly off wall)
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate to face inwards
		model = glm::scale(model, glm::vec3(4.0f, 4.0f, 4.0f)); // Scale it to be 2x2 meters
		spriteShader.setMat4("model", model);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		// Draw Right Image
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(5.4f, 2.0f, 30.0f)); // Position: Right wall (slightly off wall)
		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate to face inwards
		model = glm::scale(model, glm::vec3(4.0f, 4.0f, 4.0f));
		spriteShader.setMat4("model", model);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		glDisable(GL_BLEND); // Disable blending

		simpleCubeShader.use();
		simpleCubeShader.setMat4("projection", projection);
		simpleCubeShader.setMat4("view", view);

		// --- Draw all cubes ---
		glBindVertexArray(cubeVAO);

		animShader.use();

		// Draw all small enemies (if alive)
		for (int i = 0; i < enemies.size(); ++i)
		{
			if (enemies[i].isAlive)
			{
				// 1. Get transforms from this enemy's specific animator
				auto transforms = enemies[i].animator.GetFinalBoneMatrices();
				for (int j = 0; j < transforms.size(); ++j)
					animShader.setMat4("finalBonesMatrices[" + std::to_string(j) + "]", transforms[j]);

				// 2. Set model matrix (position, rotation, scale)
				model = glm::mat4(1.0f);

				glm::vec3 enemyModelPos = enemies[i].position;
				enemyModelPos.y -= 1.5f;
				model = glm::translate(model, enemyModelPos);
				model = glm::rotate(model, glm::radians(-10.0f), glm::vec3(0.0f, 1.0f, 0.0f)); 
				model = glm::scale(model, glm::vec3(1.5f));

				animShader.setMat4("model", model);

				// 3. Draw the model
				enemyModel.Draw(animShader);
			}
		}

		animShader.use();
		animShader.setMat4("projection", projection);
		animShader.setMat4("view", view);
		animShader.setInt("texture_diffuse1", 0);

		// Draw Boss (if active and alive)
		if (bossBattleActive && boss.isAlive) {				
			// --- New Skeletal Model Rendering ---
			glm::vec3 bossRenderPos = boss.position;
			if (bossFightStarted)
			{
				bossRenderPos.y -= 13.5f;
				bossAnimator.UpdateAnimation(deltaTime);
			}

			auto transforms = bossAnimator.GetFinalBoneMatrices();
			for (int i = 0; i < transforms.size(); ++i)
				animShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);

			// Set Model Matrix
			model = glm::mat4(1.0f);
			model = glm::translate(model, bossRenderPos);
			if (bossFightStarted)
			{
				model = glm::rotate(model, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			}
			model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::scale(model, glm::vec3(2.0f));

			animShader.setMat4("model", model);
			bossAttackModel.Draw(animShader);
		}

		simpleCubeShader.use();
		simpleCubeShader.setMat4("projection", projection);
		simpleCubeShader.setMat4("view", view);

		// Draw projectiles
		for (const auto& proj : projectiles) {
			model = glm::mat4(1.0f);
			model = glm::translate(model, proj.position);

			if (proj.isBossProjectile)
			{
				tunnelShader.use();
				tunnelShader.setMat4("projection", projection);
				tunnelShader.setMat4("view", view);

				// You may need to adjust scale or add rotation here
				// Example: Rotate it 90 degrees on Y
				model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
				model = glm::scale(model, glm::vec3(7.0f, 7.0f, 7.0f)); // Scale the model

				tunnelShader.setMat4("model", model);
				bulletModel.Draw(tunnelShader); // Draw the loaded bullet model
			}
			else
			{
				simpleCubeShader.use();
				simpleCubeShader.setMat4("projection", projection);
				simpleCubeShader.setMat4("view", view);

				model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f)); // Normal bullet
				if (proj.isParried) {
					simpleCubeShader.setVec3("ourColor", 0.0f, 1.0f, 1.0f); // Also Cyan
				}
				else {
					simpleCubeShader.setVec3("ourColor", 1.0f, 1.0f, 0.0f); // Yellow
				}

				simpleCubeShader.setMat4("model", model);
				glBindVertexArray(cubeVAO); // Re-bind the cube VAO
				glDrawArrays(GL_TRIANGLES, 0, 36); // Draw the cube
			}
		}

		// --- Draw Trampoline ---
		if (!bossBattleActive) {
			if (enemiesDefeatedCount == enemies.size())
			{
				tunnelShader.use();
				tunnelShader.setMat4("projection", projection);
				tunnelShader.setMat4("view", view);

				model = glm::mat4(1.0f);
				model = glm::translate(model, trampolinePos);
				model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				model = glm::scale(model, glm::vec3(1.0f));

				tunnelShader.setMat4("model", model);
				trampolineModel.Draw(tunnelShader);

				model = glm::mat4(1.0f);
				model = glm::translate(model, glm::vec3(0.0f, 0.0f, 30.0f)); // Center on trampoline
				model = glm::scale(model, glm::vec3(3.5f));

				tunnelShader.setMat4("model", model);
				trampolineRoomModel.Draw(tunnelShader); // This draws the room
			}
			
		}

		// --- Draw the character model ---
		animShader.use();
		animShader.setMat4("projection", projection);
		animShader.setMat4("view", view);

		auto transforms = animator.GetFinalBoneMatrices();
		for (int i = 0; i < transforms.size(); ++i)
			animShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);

		model = glm::mat4(1.0f);
		// Note: We move the model down by half the *collision height* to plant its feet
		glm::vec3 modelPos = glm::vec3(characterPos.x, characterPos.y - playerSize.y / 2.0f, characterPos.z);
		model = glm::translate(model, modelPos);
		model = glm::rotate(model, glm::radians(-characterYaw + 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f));
		animShader.setMat4("model", model);
		ourModel.Draw(animShader);


		// --- 2D HUD Rendering Pass ---
		// This renders the boss health bar on top of the 3D scene
		if (bossBattleActive && boss.isAlive)
		{
			glDisable(GL_DEPTH_TEST); // Render on top of everything
			hudShader.use();
			// Set up 2D orthographic camera
			glm::mat4 ortho = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT);
			hudShader.setMat4("projection", ortho);

			float barWidth = 400.0f;
			float barHeight = 20.0f;
			float barX = (SCR_WIDTH - barWidth) / 2.0f;
			float barY = SCR_HEIGHT - 40.0f; // 40px from top

			// 1. Draw Red Background Bar (full width)
			model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(barX, barY, 0.0f));
			model = glm::scale(model, glm::vec3(barWidth, barHeight, 1.0f));
			hudShader.setMat4("model", model);
			hudShader.setVec3("color", 1.0f, 0.0f, 0.0f); // Red
			glBindVertexArray(hudVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. Draw Green Foreground Bar (scaled by health)
			float healthPercent = boss.health / boss.maxHealth;
			model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(barX, barY, 0.0f));
			model = glm::scale(model, glm::vec3(barWidth * healthPercent, barHeight, 1.0f));
			hudShader.setMat4("model", model);
			hudShader.setVec3("color", 0.0f, 1.0f, 0.0f); // Green
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}


		// --- End of Frame ---
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// --- Cleanup ---
	glDeleteVertexArrays(1, &cubeVAO);
	glDeleteBuffers(1, &cubeVBO);
	glDeleteVertexArrays(1, &planeVAO);
	glDeleteBuffers(1, &planeVBO);
	glDeleteVertexArrays(1, &arenaVAO);
	glDeleteBuffers(1, &arenaVBO);
	glDeleteVertexArrays(1, &hudVAO);
	glDeleteBuffers(1, &hudVBO);
	glDeleteVertexArrays(1, &spriteVAO);
	glDeleteBuffers(1, &spriteVBO);

	if (walkingSound)
		walkingSound->drop();
	if (soundEngine)
		soundEngine->drop();

	glfwTerminate();
	return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
	// --- Quit ---
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	isMoving = false;
	isRunning = false;

	// Only allow movement if player is alive
	if (isPlayerAlive)
	{
		// --- Movement Speed ---
		float speed = characterSpeed;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
			speed *= 2.0f;
			isRunning = true;
		}

		// --- Movement Direction ---
		// Calculate front vector
		glm::vec3 front;
		front.x = cos(glm::radians(characterYaw));
		front.y = 0.0f;
		front.z = sin(glm::radians(characterYaw));
		front = glm::normalize(front);
		// Calculate right vector
		glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			characterPos += front * speed * deltaTime;
			isMoving = true;
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			characterPos -= front * speed * deltaTime;
			isMoving = true;
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			characterPos -= right * speed * deltaTime;
			isMoving = true;
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			characterPos += right * speed * deltaTime;
			isMoving = true;
		}


		if (walkingSound) {
			if (isRunning && isMoving)
			{
				if (walkingSound->getIsPaused())
					walkingSound->setIsPaused(false); // Play
			}
			else
			{
				if (!walkingSound->getIsPaused())
					walkingSound->setIsPaused(true); // Pause
			}
		}

		// --- Jump ---
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && canJump)
		{
			characterVelocityY = JUMP_POWER;
			canJump = false;
		}

		// --- Parry ---
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
			if (parryTimer <= 0.0f && !parryKeyPressed) {
				parryTimer = parryDuration;
				parryKeyPressed = true;
				std::cout << "*Parry Stance*" << std::endl; // NEW: Feedback
			}
		}
		else {
			parryKeyPressed = false; // Allow parry again once key is released
		}
	}
	else {
		if (walkingSound && !walkingSound->getIsPaused())
			walkingSound->setIsPaused(true);
	}
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
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
	lastX = xpos;
	lastY = ypos;

	if (isPlayerAlive) {
		float sensitivity = 0.1f;
		xoffset *= sensitivity;
		characterYaw += xoffset; // Update player's look direction
	}
}