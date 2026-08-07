/**
 * 🎮 PetBits - PocketBase Client
 * Handles all backend communication
 */

// ==========================================
// 🔧 POCKETBASE SETUP
// ==========================================

// NOTA: Cambia esta URL cuando despliegues a Fly.io
const PB_URL = 'http://127.0.0.1:8090'; // Local development
// const PB_URL = 'https://petbits-backend.fly.dev'; // Production

const pb = new PocketBase(PB_URL);

// Auto-refresh authentication
pb.autoCancellation(false);

// ==========================================
// 📦 AUTH API
// ==========================================

const PBAuth = {
    /**
     * Register a new user
     */
    async register(username, email, password) {
        try {
            // Create user account
            const user = await pb.collection('users').create({
                username,
                email,
                password,
                passwordConfirm: password,
                emailVisibility: false
            });

            // Auto-login
            await pb.collection('users').authWithPassword(email, password);

            // Create profile
            await pb.collection('profiles').create({
                user: user.id,
                coins: 100,
                totalPetsOwned: 0
            });

            // Create inventory
            await pb.collection('inventory').create({
                user: user.id,
                apple: 3,
                carrot: 2,
                meat: 1,
                cake: 0,
                star: 0
            });

            return { success: true, user };
        } catch (error) {
            console.error('Register error:', error);
            return { success: false, error: error.message };
        }
    },

    /**
     * Login existing user
     */
    async login(email, password) {
        try {
            const authData = await pb.collection('users').authWithPassword(email, password);
            return { success: true, user: authData.record };
        } catch (error) {
            console.error('Login error:', error);
            return { success: false, error: 'Email o contraseña incorrectos' };
        }
    },

    /**
     * Logout current user
     */
    logout() {
        pb.authStore.clear();
    },

    /**
     * Check if user is authenticated
     */
    isAuthenticated() {
        return pb.authStore.isValid;
    },

    /**
     * Get current user
     */
    getCurrentUser() {
        return pb.authStore.model;
    }
};

// ==========================================
// 🐾 GAME DATA API
// ==========================================

const PBGame = {
    /**
     * Get user profile (coins, etc.)
     */
    async getProfile() {
        try {
            const userId = pb.authStore.model.id;
            const profiles = await pb.collection('profiles').getFullList({
                filter: `user = "${userId}"`
            });
            return profiles[0] || null;
        } catch (error) {
            console.error('Get profile error:', error);
            return null;
        }
    },

    /**
     * Update profile data
     */
    async updateProfile(data) {
        try {
            const profile = await this.getProfile();
            if (!profile) return { success: false };

            await pb.collection('profiles').update(profile.id, data);
            return { success: true };
        } catch (error) {
            console.error('Update profile error:', error);
            return { success: false, error };
        }
    },

    /**
     * Get all user pets
     */
    async getPets() {
        try {
            const userId = pb.authStore.model.id;
            return await pb.collection('pets').getFullList({
                filter: `user = "${userId}"`,
                sort: '-created'
            });
        } catch (error) {
            console.error('Get pets error:', error);
            return [];
        }
    },

    /**
     * Get active pet
     */
    async getActivePet() {
        try {
            const userId = pb.authStore.model.id;
            const pets = await pb.collection('pets').getFullList({
                filter: `user = "${userId}" && isActive = true`
            });
            return pets[0] || null;
        } catch (error) {
            console.error('Get active pet error:', error);
            return null;
        }
    },

    /**
     * Create a new pet
     */
    async createPet(petType, name) {
        try {
            const userId = pb.authStore.model.id;
            
            // Deactivate all other pets
            const existingPets = await this.getPets();
            for (const pet of existingPets) {
                if (pet.isActive) {
                    await pb.collection('pets').update(pet.id, { isActive: false });
                }
            }

            // Create new pet
            const newPet = await pb.collection('pets').create({
                user: userId,
                type: petType,
                name: name,
                hunger: 50,
                xp: 0,
                stage: 0,
                isActive: true,
                lastFed: new Date().toISOString()
            });

            // Update profile
            const profile = await this.getProfile();
            await this.updateProfile({
                totalPetsOwned: profile.totalPetsOwned + 1
            });

            return { success: true, pet: newP };
        } catch (error) {
            console.error('Create pet error:', error);
            return { success: false, error };
        }
    },

    /**
     * Update pet data
     */
    async updatePet(petId, data) {
        try {
            await pb.collection('pets').update(petId, data);
            return { success: true };
        } catch (error) {
            console.error('Update pet error:', error);
            return { success: false, error };
        }
    },

    /**
     * Get inventory
     */
    async getInventory() {
        try {
            const userId = pb.authStore.model.id;
            const inventories = await pb.collection('inventory').getFullList({
                filter: `user = "${userId}"`
            });
            return inventories[0] || null;
        } catch (error) {
            console.error('Get inventory error:', error);
            return null;
        }
    },

    /**
     * Update inventory
     */
    async updateInventory(data) {
        try {
            const inventory = await this.getInventory();
            if (!inventory) return { success: false };

            await pb.collection('inventory').update(inventory.id, data);
            return { success: true };
        } catch (error) {
            console.error('Update inventory error:', error);
            return { success: false, error };
        }
    }
};

// ==========================================
// 🔄 REAL-TIME SYNC
// ==========================================

const PBRealtime = {
    subscriptions: [],

    /**
     * Subscribe to profile changes
     */
    subscribeToProfile(callback) {
        const userId = pb.authStore.model?.id;
        if (!userId) return;

        pb.collection('profiles').subscribe('*', (e) => {
            if (e.record.user === userId) {
                callback(e.record);
            }
        });
    },

    /**
     * Subscribe to active pet changes
     */
    subscribeToActivePet(callback) {
        const userId = pb.authStore.model?.id;
        if (!userId) return;

        pb.collection('pets').subscribe('*', (e) => {
            if (e.record.user === userId && e.record.isActive) {
                callback(e.record);
            }
        });
    },

    /**
     * Unsubscribe from all
     */
    unsubscribeAll() {
        pb.collection('profiles').unsubscribe();
        pb.collection('pets').unsubscribe();
        pb.collection('inventory').unsubscribe();
    }
};

// Export for use in main.js
window.PBAuth = PBAuth;
window.PBGame = PBGame;
window.PBRealtime = PBRealtime;
window.pb = pb;
