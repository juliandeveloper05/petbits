/**
 * 🎮 PETBITS - Virtual Pet Simulator
 * Videojuego vintage de mascotas virtuales
 * Desarrollado con amor retro ❤️
 */

document.addEventListener('DOMContentLoaded', () => {
    // ==========================================
    // 🎯 GAME STATE
    // ==========================================
    const gameState = {
        coins: 100,
        ownedPets: [],
        currentPet: null,
        inventory: {
            apple: 3,
            carrot: 2,
            meat: 1,
            cake: 0,
            star: 0
        },
        selectedFood: null
    };

    // Configuración de mascotas
    const petConfig = {
        puppy: { 
            name: 'Perrito', 
            emoji: '🐶', 
            stages: ['🐕', '🐕‍🦺', '🦮'],
            price: 50 
        },
        kitten: { 
            name: 'Gatito', 
            emoji: '🐱', 
            stages: ['🐈', '🐈‍⬛', '🦁'],
            price: 50 
        },
        bunny: { 
            name: 'Conejito', 
            emoji: '🐰', 
            stages: ['🐇', '🐇', '🐰'],
            price: 60 
        },
        hamster: { 
            name: 'Hamster', 
            emoji: '🐹', 
            stages: ['🐹', '🐹', '🐿️'],
            price: 40 
        },
        dragon: { 
            name: 'Dragón', 
            emoji: '🐲', 
            stages: ['🐉', '🐲', '🔥'],
            price: 200,
            special: true
        },
        unicorn: { 
            name: 'Unicornio', 
            emoji: '🦄', 
            stages: ['🦄', '🌈', '✨'],
            price: 300,
            special: true
        }
    };

    // Configuración de comida
    const foodConfig = {
        apple: { emoji: '🍎', energy: 10, xp: 5 },
        carrot: { emoji: '🥕', energy: 15, xp: 8 },
        meat: { emoji: '🍖', energy: 25, xp: 12 },
        cake: { emoji: '🍰', energy: 40, xp: 20 },
        star: { emoji: '⭐', energy: 100, xp: 50 }
    };

    // ==========================================
    // 🎨 DOM ELEMENTS
    // ==========================================
    const screens = {
        title: document.getElementById('title-screen'),
        shop: document.getElementById('shop-screen'),
        care: document.getElementById('care-screen')
    };

    const elements = {
        // Title screen
        startBtn: document.getElementById('start-btn'),
        
        // Shop screen
        coinsAmount: document.getElementById('coins-amount'),
        shopTabs: document.querySelectorAll('.tab-btn'),
        petsTab: document.getElementById('pets-tab'),
        foodTab: document.getElementById('food-tab'),
        shopItems: document.querySelectorAll('.shop-item'),
        foodItems: document.querySelectorAll('.food-item'),
        myPetsBtn: document.getElementById('my-pets-btn'),
        
        // Care screen
        backToShopBtn: document.getElementById('back-to-shop-btn'),
        coinsAmountCare: document.getElementById('coins-amount-care'),
        petNameHeader: document.getElementById('pet-name-header'),
        stageBadge: document.getElementById('stage-badge'),
        petSprite: document.getElementById('pet-sprite'),
        petReaction: document.getElementById('pet-reaction'),
        levelUpEffect: document.getElementById('level-up-effect'),
        hungerFill: document.getElementById('hunger-fill'),
        hungerValue: document.getElementById('hunger-value'),
        xpFill: document.getElementById('xp-fill'),
        xpValue: document.getElementById('xp-value'),
        growthStages: document.querySelectorAll('.stage'),
        foodInventory: document.getElementById('food-inventory'),
        feedBtn: document.getElementById('feed-btn'),
        playBtn: document.getElementById('play-btn'),
        sleepBtn: document.getElementById('sleep-btn'),
        
        // Modal
        foodModal: document.getElementById('food-modal'),
        modalFoodGrid: document.getElementById('modal-food-grid'),
        closeFoodModal: document.getElementById('close-food-modal'),
        
        // Toast
        toast: document.getElementById('toast'),
        toastMessage: document.getElementById('toast-message')
    };

    // ==========================================
    // 🔧 UTILITY FUNCTIONS
    // ==========================================
    function showScreen(screenName) {
        Object.values(screens).forEach(screen => {
            screen.classList.remove('active');
        });
        screens[screenName].classList.add('active');
    }

    function showToast(message, type = 'info') {
        elements.toast.className = 'toast show ' + type;
        elements.toastMessage.textContent = message;
        
        setTimeout(() => {
            elements.toast.classList.remove('show');
        }, 2500);
    }

    function updateCoinsDisplay() {
        elements.coinsAmount.textContent = gameState.coins;
        elements.coinsAmountCare.textContent = gameState.coins;
    }

    function playSound(type) {
        // Placeholder para efectos de sonido
        // En una versión completa, aquí iría la lógica de audio
        console.log(`🔊 Sound: ${type}`);
    }

    // ==========================================
    // 🛒 SHOP LOGIC
    // ==========================================
    function initShop() {
        // Tab switching
        elements.shopTabs.forEach(tab => {
            tab.addEventListener('click', () => {
                elements.shopTabs.forEach(t => t.classList.remove('active'));
                tab.classList.add('active');
                
                const tabName = tab.dataset.tab;
                document.querySelectorAll('.tab-content').forEach(content => {
                    content.classList.remove('active');
                });
                document.getElementById(`${tabName}-tab`).classList.add('active');
            });
        });

        // Pet purchase
        elements.shopItems.forEach(item => {
            const buyBtn = item.querySelector('.buy-btn');
            buyBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                const petType = item.dataset.pet;
                const price = parseInt(item.dataset.price);
                
                if (gameState.coins >= price) {
                    purchasePet(petType, price);
                } else {
                    showToast('¡No tienes suficientes monedas! 💔', 'error');
                }
            });
        });

        // Food quantity controls
        elements.foodItems.forEach(item => {
            const minusBtn = item.querySelector('.minus');
            const plusBtn = item.querySelector('.plus');
            const qtyDisplay = item.querySelector('.qty-display');
            const foodType = item.dataset.food;
            const price = parseInt(item.dataset.price);

            minusBtn.addEventListener('click', () => {
                let qty = parseInt(qtyDisplay.textContent);
                if (qty > 0) {
                    qty--;
                    qtyDisplay.textContent = qty;
                }
            });

            plusBtn.addEventListener('click', () => {
                let qty = parseInt(qtyDisplay.textContent);
                const maxAffordable = Math.floor(gameState.coins / price);
                if (qty < maxAffordable) {
                    qty++;
                    qtyDisplay.textContent = qty;
                } else {
                    showToast('¡No puedes comprar más!', 'error');
                }
            });
        });

        // Confirm food purchase when switching tabs
        elements.shopTabs.forEach(tab => {
            tab.addEventListener('click', () => {
                if (tab.dataset.tab === 'pets') {
                    completeFoodPurchase();
                }
            });
        });
    }

    function purchasePet(petType, price) {
        gameState.coins -= price;
        updateCoinsDisplay();
        
        const newPet = createPet(petType);
        gameState.ownedPets.push(newPet);
        gameState.currentPet = newPet;
        
        showToast(`¡Compraste un ${petConfig[petType].name}! 🎉`, 'success');
        playSound('purchase');
        
        // Ir a la pantalla de cuidado
        setTimeout(() => {
            showScreen('care');
            initCareScreen();
        }, 1000);
    }

    function createPet(petType) {
        return {
            id: Date.now(),
            type: petType,
            name: petConfig[petType].name,
            hunger: 50,
            xp: 0,
            stage: 0, // 0: baby, 1: young, 2: adult
            stageNames: ['BEBÉ', 'JOVEN', 'ADULTO'],
            xpToNextStage: [100, 250, Infinity],
            createdAt: new Date()
        };
    }

    function completeFoodPurchase() {
        let totalCost = 0;
        let itemsBought = [];

        elements.foodItems.forEach(item => {
            const qtyDisplay = item.querySelector('.qty-display');
            const qty = parseInt(qtyDisplay.textContent);
            const price = parseInt(item.dataset.price);
            const foodType = item.dataset.food;

            if (qty > 0) {
                const cost = qty * price;
                totalCost += cost;
                gameState.inventory[foodType] += qty;
                itemsBought.push({ type: foodType, qty });
                qtyDisplay.textContent = '0';
            }
        });

        if (totalCost > 0) {
            gameState.coins -= totalCost;
            updateCoinsDisplay();
            showToast(`¡Compraste comida por ${totalCost} monedas!`, 'success');
        }
    }

    // ==========================================
    // 🐾 CARE SCREEN LOGIC
    // ==========================================
    let hungerInterval = null;
    let coinInterval = null;

    function initCareScreen() {
        if (!gameState.currentPet) return;

        const pet = gameState.currentPet;
        
        // Update UI
        elements.petNameHeader.textContent = pet.name;
        updatePetSprite();
        updateStats();
        updateInventoryDisplay();
        updateGrowthStages();
        
        // Start hunger decay
        startHungerDecay();
        
        // Start coin generation
        startCoinGeneration();
    }

    function updatePetSprite() {
        const pet = gameState.currentPet;
        const config = petConfig[pet.type];
        const sprite = elements.petSprite.querySelector('.sprite-emoji');
        
        // Update emoji based on stage
        sprite.textContent = config.stages[pet.stage] || config.emoji;
        
        // Update size class
        elements.petSprite.classList.remove('baby', 'young', 'adult');
        elements.petSprite.classList.add(['baby', 'young', 'adult'][pet.stage]);
        
        // Update badge
        elements.stageBadge.textContent = pet.stageNames[pet.stage];
    }

    function updateStats() {
        const pet = gameState.currentPet;
        
        // Hunger
        elements.hungerFill.style.width = `${pet.hunger}%`;
        elements.hungerValue.textContent = `${Math.round(pet.hunger)}%`;
        
        // Change color based on hunger level
        if (pet.hunger < 30) {
            elements.hungerFill.style.background = 'linear-gradient(90deg, #ff3864 0%, #ff6b9d 100%)';
        } else if (pet.hunger < 60) {
            elements.hungerFill.style.background = 'linear-gradient(90deg, #ffa500 0%, #ffd700 100%)';
        } else {
            elements.hungerFill.style.background = 'linear-gradient(90deg, #00ff88 0%, #00cc6a 100%)';
        }
        
        // XP
        const xpNeeded = pet.xpToNextStage[pet.stage];
        const xpPercent = pet.stage >= 2 ? 100 : (pet.xp / xpNeeded) * 100;
        elements.xpFill.style.width = `${xpPercent}%`;
        elements.xpValue.textContent = pet.stage >= 2 ? 'MAX' : `${pet.xp}/${xpNeeded}`;
    }

    function updateInventoryDisplay() {
        Object.keys(gameState.inventory).forEach(food => {
            const qtyElement = document.getElementById(`inv-${food}`);
            if (qtyElement) {
                qtyElement.textContent = gameState.inventory[food];
            }
        });
    }

    function updateGrowthStages() {
        const pet = gameState.currentPet;
        
        elements.growthStages.forEach((stage, index) => {
            stage.classList.remove('active', 'completed');
            if (index === pet.stage) {
                stage.classList.add('active');
            } else if (index < pet.stage) {
                stage.classList.add('completed');
            }
        });
    }

    function startHungerDecay() {
        if (hungerInterval) clearInterval(hungerInterval);
        
        hungerInterval = setInterval(() => {
            if (!gameState.currentPet) return;
            
            gameState.currentPet.hunger = Math.max(0, gameState.currentPet.hunger - 0.5);
            updateStats();
            
            // Warning when hungry
            if (gameState.currentPet.hunger <= 20 && gameState.currentPet.hunger > 19) {
                showToast('¡Tu mascota tiene hambre! 🍽️', 'error');
                showReaction('😢');
            }
            
            // Critical hunger
            if (gameState.currentPet.hunger <= 0) {
                elements.petSprite.style.opacity = '0.5';
            } else {
                elements.petSprite.style.opacity = '1';
            }
        }, 1000);
    }

    function startCoinGeneration() {
        if (coinInterval) clearInterval(coinInterval);
        
        // Earn coins passively based on pet stage
        coinInterval = setInterval(() => {
            if (!gameState.currentPet || gameState.currentPet.hunger <= 0) return;
            
            const coinsEarned = gameState.currentPet.stage + 1;
            gameState.coins += coinsEarned;
            updateCoinsDisplay();
        }, 5000); // Every 5 seconds
    }

    function feedPet(foodType) {
        if (!gameState.currentPet) return;
        if (gameState.inventory[foodType] <= 0) {
            showToast('¡No tienes ese alimento!', 'error');
            return;
        }

        const pet = gameState.currentPet;
        const food = foodConfig[foodType];

        // Consume food
        gameState.inventory[foodType]--;
        updateInventoryDisplay();

        // Apply effects
        pet.hunger = Math.min(100, pet.hunger + food.energy);
        addXP(food.xp);

        // Animations
        elements.petSprite.classList.add('feeding');
        showReaction('😋');
        
        setTimeout(() => {
            elements.petSprite.classList.remove('feeding');
        }, 1000);

        updateStats();
        playSound('eat');
        
        // Close modal
        elements.foodModal.classList.remove('show');
    }

    function addXP(amount) {
        const pet = gameState.currentPet;
        pet.xp += amount;

        // Check for level up
        if (pet.stage < 2 && pet.xp >= pet.xpToNextStage[pet.stage]) {
            levelUp();
        }
    }

    function levelUp() {
        const pet = gameState.currentPet;
        pet.xp = 0;
        pet.stage++;

        // Show level up effect
        elements.levelUpEffect.classList.add('show');
        setTimeout(() => {
            elements.levelUpEffect.classList.remove('show');
        }, 2000);

        updatePetSprite();
        updateStats();
        updateGrowthStages();

        showToast(`¡${pet.name} ha crecido! 🌟`, 'success');
        playSound('levelup');

        // Bonus coins on level up
        const bonusCoins = (pet.stage + 1) * 25;
        gameState.coins += bonusCoins;
        updateCoinsDisplay();
        showToast(`+${bonusCoins} monedas de bonus! 🪙`, 'success');
    }

    function showReaction(emoji) {
        elements.petReaction.textContent = emoji;
        elements.petReaction.classList.add('show');
        
        setTimeout(() => {
            elements.petReaction.classList.remove('show');
        }, 1500);
    }

    function playWithPet() {
        if (!gameState.currentPet || gameState.currentPet.hunger < 10) {
            showToast('¡Tu mascota necesita comer primero!', 'error');
            return;
        }

        const pet = gameState.currentPet;
        
        // Cost: some hunger
        pet.hunger = Math.max(0, pet.hunger - 10);
        
        // Gain: XP
        addXP(15);
        
        // Animations
        elements.petSprite.classList.add('playing');
        showReaction('🎾');
        
        setTimeout(() => {
            elements.petSprite.classList.remove('playing');
        }, 2000);

        updateStats();
        playSound('play');
    }

    function sleepPet() {
        if (!gameState.currentPet) return;

        const pet = gameState.currentPet;
        
        // Sleeping animation
        elements.petSprite.classList.add('sleeping');
        showReaction('💤');
        
        // Disable buttons temporarily
        elements.feedBtn.disabled = true;
        elements.playBtn.disabled = true;
        elements.sleepBtn.disabled = true;
        
        // Sleep for 3 seconds, recover hunger
        let sleepTime = 0;
        const sleepInterval = setInterval(() => {
            sleepTime++;
            pet.hunger = Math.min(100, pet.hunger + 2);
            updateStats();
            
            if (sleepTime >= 3) {
                clearInterval(sleepInterval);
                elements.petSprite.classList.remove('sleeping');
                elements.feedBtn.disabled = false;
                elements.playBtn.disabled = false;
                elements.sleepBtn.disabled = false;
                showReaction('😊');
            }
        }, 1000);

        playSound('sleep');
    }

    function openFoodModal() {
        // Populate modal with available food
        elements.modalFoodGrid.innerHTML = '';
        
        Object.entries(gameState.inventory).forEach(([food, qty]) => {
            const foodInfo = foodConfig[food];
            const item = document.createElement('div');
            item.className = `modal-food-item ${qty <= 0 ? 'disabled' : ''}`;
            item.innerHTML = `
                <span class="modal-food-icon ${food === 'star' ? 'rainbow' : ''}">${foodInfo.emoji}</span>
                <span class="modal-food-qty">x${qty}</span>
            `;
            
            if (qty > 0) {
                item.addEventListener('click', () => feedPet(food));
            }
            
            elements.modalFoodGrid.appendChild(item);
        });
        
        elements.foodModal.classList.add('show');
    }

    // ==========================================
    // 🎮 EVENT LISTENERS
    // ==========================================
    
    // Title screen
    elements.startBtn.addEventListener('click', () => {
        showScreen('shop');
        playSound('start');
    });

    // Shop screen
    elements.myPetsBtn.addEventListener('click', () => {
        completeFoodPurchase();
        
        if (gameState.currentPet) {
            showScreen('care');
            initCareScreen();
        } else if (gameState.ownedPets.length > 0) {
            gameState.currentPet = gameState.ownedPets[gameState.ownedPets.length - 1];
            showScreen('care');
            initCareScreen();
        } else {
            showToast('¡Primero debes comprar una mascota!', 'error');
        }
    });

    // Care screen
    elements.backToShopBtn.addEventListener('click', () => {
        showScreen('shop');
    });

    elements.feedBtn.addEventListener('click', openFoodModal);
    elements.playBtn.addEventListener('click', playWithPet);
    elements.sleepBtn.addEventListener('click', sleepPet);
    elements.closeFoodModal.addEventListener('click', () => {
        elements.foodModal.classList.remove('show');
    });

    // Inventory item selection
    document.querySelectorAll('.inv-item').forEach(item => {
        item.addEventListener('click', () => {
            const food = item.dataset.food;
            if (gameState.inventory[food] > 0) {
                feedPet(food);
            } else {
                showToast('¡No tienes este alimento!', 'error');
            }
        });
    });

    // Pet sprite click - random reaction
    elements.petSprite.addEventListener('click', () => {
        const reactions = ['❤️', '😊', '🎵', '✨', '💕', '🌟'];
        const randomReaction = reactions[Math.floor(Math.random() * reactions.length)];
        showReaction(randomReaction);
        addXP(1); // Small XP for interaction
    });

    // Console buttons (decorative but functional)
    document.querySelector('.btn-a').addEventListener('click', () => {
        if (screens.care.classList.contains('active')) {
            openFoodModal();
        }
    });

    document.querySelector('.btn-b').addEventListener('click', () => {
        if (screens.care.classList.contains('active')) {
            showScreen('shop');
        } else if (screens.shop.classList.contains('active')) {
            showScreen('title');
        }
    });

    // ==========================================
    // 🚀 INITIALIZATION
    // ==========================================
    function init() {
        updateCoinsDisplay();
        initShop();
        
        // Load saved game if exists
        loadGame();
        
        console.log('🎮 PetBits initialized!');
    }

    // Save/Load functionality
    function saveGame() {
        const saveData = {
            coins: gameState.coins,
            inventory: gameState.inventory,
            ownedPets: gameState.ownedPets,
            currentPetId: gameState.currentPet?.id
        };
        localStorage.setItem('petbits_save', JSON.stringify(saveData));
    }

    function loadGame() {
        const savedData = localStorage.getItem('petbits_save');
        if (savedData) {
            try {
                const data = JSON.parse(savedData);
                gameState.coins = data.coins || 100;
                gameState.inventory = data.inventory || gameState.inventory;
                gameState.ownedPets = data.ownedPets || [];
                
                if (data.currentPetId && gameState.ownedPets.length > 0) {
                    gameState.currentPet = gameState.ownedPets.find(p => p.id === data.currentPetId);
                }
                
                updateCoinsDisplay();
                console.log('📁 Game loaded!');
            } catch (e) {
                console.error('Error loading save:', e);
            }
        }
    }

    // Auto-save every 30 seconds
    setInterval(saveGame, 30000);

    // Save before closing
    window.addEventListener('beforeunload', saveGame);

    // Start the game!
    init();
});
