#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader_m.h> 

#include <iostream>
#include <vector> 

#define BASE_PATH "D:/MyWork/University/CEDT/2110582-Elec/68-GAME_EN/LearnOpenGL-master/src/8.guest/2020/skeletal_animation/"

// --- Function Prototypes ---
// GLFW callback for window resize
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
// GLFW callback for mouse movement
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
// Processes keyboard input every frame
void processInput(GLFWwindow* window);

// --- Game Settings ---
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// --- Character & Camera ---
glm::vec3 characterPos = glm::vec3(0.0f, 1.0f, 55.0f); // Character's center
glm::vec3 characterSpawnPos = glm::vec3(0.0f, 1.0f, 55.0f); // Spawn point
float characterYaw = -90.0f; // Start facing down the -Z axis
float characterSpeed = 5.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool isPlayerAlive = true; // Game state
int playerLives = 3; // Player lives 

// --- Jump Physics ---
float characterVelocityY = 0.0f;
const float GRAVITY = -18.0f;
const float JUMP_POWER = 8.0f;
bool canJump = true;

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
	glm::vec3 position = glm::vec3(0.0f, 2.5f, -70.0f);
	glm::vec3 size = glm::vec3(5.0f, 5.0f, 5.0f);
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
const float endOfRunwayZ = -60.0f; // Z-coordinate to trigger boss

// --- World Boundaries ---
const float RUNWAY_MIN_X = -5.0f;
const float RUNWAY_MAX_X = 5.0f;
const float RUNWAY_MIN_Z = -60.5f; // Slightly past trigger
const float RUNWAY_MAX_Z = 60.0f;

const float ARENA_MIN_X = -20.0f;
const float ARENA_MAX_X = 20.0f;
const float ARENA_MIN_Z = -90.0f;
const float ARENA_MAX_Z = -60.0f; // Connects to runway

// --- Gameplay Constants ---
const float shootCooldown = 2.0f;
const float bossShootCooldown = 2.0f;
const float projectileSpeed = 15.0f;
const float bossComboWindow = 5.0f; // 5 seconds to continue combo

// --- Timing ---
float deltaTime = 0.0f;
float lastFrame = 0.0f;

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

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Cave Combat", NULL, NULL);
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

	// Enable depth testing so objects don't draw through each other
	glEnable(GL_DEPTH_TEST);

	// --- Shader Compilation ---
	Shader ourShader("anim_model.vs", "anim_model.fs"); // For 3D objects
	Shader hudShader(BASE_PATH "hud.vs", BASE_PATH "hud.fs"); // For 2D health bar

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
		 5.0f, 0.0f,  60.0f, -5.0f, 0.0f, -60.0f, -5.0f, 0.0f,  60.0f,
		 5.0f, 0.0f,  60.0f,  5.0f, 0.0f, -60.0f, -5.0f, 0.0f, -60.0f
	};
	// Vertex data for the boss arena plane (set slightly lower to prevent Z-fighting)
	float arenaVertices[] = {
		 20.0f, -0.01f, -60.0f, -20.0f, -0.01f, -90.0f, -20.0f, -0.01f, -60.0f,
		 20.0f, -0.01f, -60.0f,  20.0f, -0.01f, -90.0f, -20.0f, -0.01f, -90.0f
	};
	// Vertex data for the 2D HUD (a simple 2D quad)
	float hudVertices[] = {
		0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f
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
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// --- Boss Arena VAO/VBO ---
	unsigned int arenaVAO, arenaVBO;
	glGenVertexArrays(1, &arenaVAO);
	glGenBuffers(1, &arenaVBO);
	glBindVertexArray(arenaVAO);
	glBindBuffer(GL_ARRAY_BUFFER, arenaVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(arenaVertices), arenaVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// --- HUD VAO/VBO ---
	unsigned int hudVAO, hudVBO;
	glGenVertexArrays(1, &hudVAO);
	glGenBuffers(1, &hudVBO);
	glBindVertexArray(hudVAO);
	glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(hudVertices), hudVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

	// --- Game Object Initialization ---
	enemies.push_back(Enemy{ glm::vec3(7.0f, 1.0f, 40.0f) });
	enemies.push_back(Enemy{ glm::vec3(-7.0f, 1.0f, 20.0f) });
	enemies.push_back(Enemy{ glm::vec3(7.0f, 1.0f, -20.0f) });

	// --- NEW: Welcome Message ---
	std::cout << "=============================================" << std::endl;
	std::cout << "         WELCOME TO THE CAVE COMBAT!         " << std::endl;
	std::cout << "=============================================" << std::endl;
	std::cout << "Objective: You are assigned to fight with the" << std::endl;
	std::cout << "           'Great Mole' of the cave. " << std::endl;
	std::cout << "           Let's see if you can do it or not!" << std::endl;
	std::cout << "\nControls:" << std::endl;
	std::cout << "  W, A, S, D - Move" << std::endl;
	std::cout << "  Shift      - Run" << std::endl;
	std::cout << "  E          - Parry (Runway Only)" << std::endl;
	std::cout << "  Space      - Jump" << std::endl;
	std::cout << "  Mouse      - Look around" << std::endl;
	std::cout << "  ESC        - Quit the game" << std::endl;
	std::cout << "\nYou have " << playerLives << " lives. Good luck." << std::endl;
	std::cout << "---------------------------------------------" << std::endl;

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
		glm::vec3 playerSize = glm::vec3(0.5f, 2.0f, 0.5f);
		glm::vec3 enemySize = glm::vec3(1.0f, 2.0f, 1.0f);

		// --- Input ---
		processInput(window);

		// --- Physics Update (Gravity & Jump) ---
		if (!canJump) { // If player is in the air
			characterVelocityY += GRAVITY * deltaTime; // Apply gravity
			characterPos.y += characterVelocityY * deltaTime; // Update vertical position

			// Check for landing on the runway
			if (characterPos.y <= 1.0f && !bossBattleActive) {
				characterPos.y = 1.0f;
				characterVelocityY = 0.0f;
				canJump = true;
			}
			// Check for landing on the boss arena floor
			else if (bossBattleActive && characterPos.y <= 1.0f) {
				characterPos.y = 1.0f;
				characterVelocityY = 0.0f;
				canJump = true;
			}
		}

		// --- Boundary Collision (Invisible Walls) ---
		float playerHalfWidth = playerSize.x / 2.0f;
		float playerHalfDepth = playerSize.z / 2.0f;

		if (!bossBattleActive)
		{
			// Check runway boundaries
			if (characterPos.x - playerHalfWidth < RUNWAY_MIN_X || characterPos.x + playerHalfWidth > RUNWAY_MAX_X)
			{
				characterPos.x = lastSafePos.x; // Revert X
			}
			if (characterPos.z - playerHalfDepth < RUNWAY_MIN_Z || characterPos.z + playerHalfDepth > RUNWAY_MAX_Z)
			{
				characterPos.z = lastSafePos.z; // Revert Z
			}
		}
		else
		{
			// Check arena boundaries
			if (characterPos.x - playerHalfWidth < ARENA_MIN_X || characterPos.x + playerHalfWidth > ARENA_MAX_X)
			{
				characterPos.x = lastSafePos.x; // Revert X
			}
			if (characterPos.z - playerHalfDepth < ARENA_MIN_Z || characterPos.z + playerHalfDepth > ARENA_MAX_Z)
			{
				characterPos.z = lastSafePos.z; // Revert Z
			}
		}

		// --- Core Game Logic ---
		// Tick down active timers
		if (parryTimer > 0.0f) {
			parryTimer -= deltaTime;
		}
		if (boss.hitTimer > 0.0f) {
			boss.hitTimer -= deltaTime;
		}

		// --- Game State FSM (Finite State Machine) ---
		// This block switches between "Runway" logic and "Boss" logic
		if (!bossBattleActive)
		{
			// --- RUNWAY LOGIC ---

			// Check if all enemies are dead and player has reached the end
			if (enemiesDefeatedCount == enemies.size() && characterPos.z <= endOfRunwayZ)
			{
				bossBattleActive = true;
				std::cout << "---------------------------------" << std::endl;
				std::cout << "BOSS BATTLE START!" << std::endl;
				projectiles.clear(); // Clear runway projectiles

				characterPos.z -= 1.0f; // Nudge player into arena
				canJump = true; // Land player 

				// Set lives to 1 for boss battle
				if (playerLives > 1) {
					std::cout << "Your extra lives vanish... It's all or nothing." << std::endl;
					playerLives = 1;
					std::cout << "You have only " << playerLives << " live. Good Luck!" << std::endl;
				}
				std::cout << "---------------------------------" << std::endl;
			}

			// Update Small Enemies
			for (int i = 0; i < enemies.size(); ++i)
			{
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
			if (isPlayerAlive && boss.isAlive)
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
					Projectile newProjectile;
					newProjectile.position = boss.position;
					glm::vec3 direction = glm::normalize(characterPos - boss.position);
					newProjectile.velocity = direction * projectileSpeed * 1.2f; // Shoots faster
					newProjectile.enemyIndex = -1; // -1 means it's from the boss
					newProjectile.isBossProjectile = true;
					projectiles.push_back(newProjectile);
				}

				// Boss Collision and Attack Logic
				if (checkCollisionAABB(characterPos, playerSize, boss.position, boss.size))
				{
					float bossHeadY = boss.position.y + (boss.size.y / 2.0f);

					// 1. Check for Head Jump (Attack)
					// Conditions: Player is falling, player is above boss, boss attack is not on cooldown
					if (characterVelocityY < 0.0f && characterPos.y > boss.position.y && boss.hitTimer <= 0.0f)
					{
						// --- HEAD JUMP SUCCESS ---
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
						}
					}
					// 2. Check for Landing on Head (no attack)
					else if (characterVelocityY < 0.0f && characterPos.y > boss.position.y)
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
			}
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
					enemies[targetEnemyIndex].isAlive = false;
					std::cout << "ENEMY " << targetEnemyIndex << " DEFEATED!" << std::endl;
					enemiesDefeatedCount++;
					std::cout << "Enemies remaining: " << enemies.size() - enemiesDefeatedCount << std::endl;

					// --- NEW: Check if all enemies are dead ---
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
						}
						else {
							// Normal projectiles remove one life
							playerLives--;
							std::cout << "HIT! Lives remaining: " << playerLives << std::endl;

							if (playerLives > 0) {
								// Respawn at the start
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
		ourShader.use();
		glEnable(GL_DEPTH_TEST); // Make sure depth test is on for 3D

		// Set up 3D perspective camera
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
		// Calculate camera's "front" vector based on player's yaw
		glm::vec3 front;
		front.x = cos(glm::radians(characterYaw));
		front.y = 0.0f;
		front.z = sin(glm::radians(characterYaw));
		front = glm::normalize(front);

		// Position camera behind and above the player
		glm::vec3 cameraPos = characterPos - (front * 10.0f) + glm::vec3(0.0f, 5.0f, 0.0f);
		glm::mat4 view = glm::lookAt(cameraPos, characterPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		ourShader.setMat4("projection", projection);
		ourShader.setMat4("view", view);

		// Draw the runway
		glBindVertexArray(planeVAO);
		glm::mat4 model = glm::mat4(1.0f);
		ourShader.setMat4("model", model);
		ourShader.setVec3("ourColor", 0.5f, 0.5f, 0.5f); // Grey
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Draw Arena (only if boss battle started)
		if (bossBattleActive) {
			glBindVertexArray(arenaVAO);
			model = glm::mat4(1.0f);
			ourShader.setMat4("model", model);
			ourShader.setVec3("ourColor", 0.4f, 0.4f, 0.4f); // Darker Grey
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}


		// --- Draw all cubes ---
		glBindVertexArray(cubeVAO);

		// Draw the character
		model = glm::mat4(1.0f);
		model = glm::translate(model, characterPos);
		model = glm::rotate(model, glm::radians(-characterYaw - 90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Align with camera
		model = glm::scale(model, playerSize);
		if (parryTimer > 0.0f) {
			ourShader.setVec3("ourColor", 1.0f, 1.0f, 1.0f); // White when parrying
		}
		else {
			ourShader.setVec3("ourColor", 1.0f, 0.0f, 0.0f); // Red
		}
		ourShader.setMat4("model", model);
		glDrawArrays(GL_TRIANGLES, 0, 36);

		// Draw all small enemies (if alive)
		for (const auto& enemy : enemies) {
			if (enemy.isAlive) {
				model = glm::mat4(1.0f);
				model = glm::translate(model, enemy.position);
				model = glm::scale(model, enemySize);
				ourShader.setMat4("model", model);
				ourShader.setVec3("ourColor", 0.0f, 0.0f, 1.0f); // Blue
				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}

		// Draw Boss (if active and alive)
		if (bossBattleActive && boss.isAlive) {
			model = glm::mat4(1.0f);
			model = glm::translate(model, boss.position);
			model = glm::scale(model, boss.size);
			ourShader.setMat4("model", model);
			// Set color to flash white when hit
			if (boss.hitTimer > 1.8f) { // Flash for 0.2s
				ourShader.setVec3("ourColor", 1.0f, 1.0f, 1.0f); // White
			}
			else if (boss.hitCombo == 3) { // Show combo is ready
				ourShader.setVec3("ourColor", 1.0f, 0.5f, 1.0f); // Bright Pink
			}
			else {
				ourShader.setVec3("ourColor", 0.5f, 0.0f, 0.5f); // Purple
			}
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

		// Draw projectiles
		for (const auto& proj : projectiles) {
			model = glm::mat4(1.0f);
			model = glm::translate(model, proj.position);
			if (proj.isBossProjectile) {
				model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f)); // Bigger bullet
				ourShader.setVec3("ourColor", 0.0f, 1.0f, 1.0f); // Cyan
			}
			else {
				model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f)); // Normal bullet
				if (proj.isParried) {
					ourShader.setVec3("ourColor", 0.0f, 1.0f, 1.0f); // Also Cyan
				}
				else {
					ourShader.setVec3("ourColor", 1.0f, 1.0f, 0.0f); // Yellow
				}
			}
			ourShader.setMat4("model", model);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}


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

	// Only allow movement if player is alive
	if (isPlayerAlive)
	{
		// --- Movement Speed ---
		float speed = characterSpeed;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
			speed *= 2.0f; // Double speed for running

		// --- Movement Direction ---
		// Calculate front vector
		glm::vec3 front;
		front.x = cos(glm::radians(characterYaw));
		front.y = 0.0f;
		front.z = sin(glm::radians(characterYaw));
		front = glm::normalize(front);
		// Calculate right vector
		glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			characterPos += front * speed * deltaTime;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			characterPos -= front * speed * deltaTime;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			characterPos -= right * speed * deltaTime;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			characterPos += right * speed * deltaTime;

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