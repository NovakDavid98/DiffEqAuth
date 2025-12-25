import numpy as np
from scipy.integrate import solve_ivp
from simulation import ID14Params, ID26Params, id14_derivatives, id26_step_1d, compute_gradient_magnitude_2d

class LivingAgent:
    """
    A Biorhythmic Agent driven by Universe432 physics.
    
    The agent's internal state (Attention, Creativity, Action Permission)
    is governed by the coupled ID14-ID26 dynamics.
    """
    
    def __init__(self, dt=0.1):
        self.dt = dt
        self.t = 0.0
        
        # Initialize Physics Parameters
        self.params_14 = ID14Params()
        self.params_26 = ID26Params()
        
        # Customize for "Living Agent" behavior
        # We want it to sleep easily (decay) and wake up fast (coupling)
        self.params_14.gamma_psi = 0.02    # Slower decay of attention (was 0.05)
        self.params_14.k_I_phi = 2.0       # Much stronger reaction to environment (was 0.5)
        self.params_14.k_R_phi = 5.0       # Stronger drive for Action (was 0.05)
        
        # HACK: Reduce R decay implies a "more confident" agent
        # The original physic has ~20.0 decay factor. We reduce U_E effective for decay.
        # We can't easily change the equation in simulation.py, but we can change parameters.
        # dR/dt = k*Psi*I... - (2*UE/R_char)*R
        # If we make R_char HUGE, decay becomes small.
        self.params_14.R_char = 1000.0     # Reduce R decay (was ~8.0)
        
        # Fix environment sink to prevent negative drift
        self.params_26.beta_phi = 0.01     # Reduced sink (was 0.1)
        
        # Initial State: Sleeping but not dead
        self.I = 0.5
        self.R = 0.1
        self.Psi = 0.2
        
        # 1D "Environment" Field (representing a buffer of logs/events)
        self.N_x = 20
        self.dx = 1.0
        self.Phi = np.zeros(self.N_x) + 0.1 # Low background noise
        
    def update_environment(self, stress_input):
        """
        Inject energy into the environment field (Phi).
        stress_input: float (0.0 to 1.0+) representing error severity.
        """
        # We add stress to the center of the field ("the agent's focus")
        mid = self.N_x // 2
        
        # Simple injection: Add to local Phi
        self.Phi[mid] += stress_input * 2.0
        
        # Add some random noise if stress is high
        if stress_input > 0:
            noise = np.random.randn(self.N_x) * 0.1 * stress_input
            self.Phi += noise
            
        # Clamp to prevent explosion in this simple demo
        self.Phi = np.clip(self.Phi, -5.0, 5.0)

    def step(self):
        """
        Advance the internal physics by one time step.
        """
        # 1. Calculate Coupling Terms
        Phi_avg = np.mean(self.Phi)
        
        # Gradient feedback (mental friction from complex environment)
        grad_Phi = np.abs(np.roll(self.Phi, -1) - np.roll(self.Phi, 1)) / (2*self.dx)
        Psi_d_feedback = np.mean(grad_Phi)
        
        # 2. Evolve Mind (ID14)
        # We use a single RK45 step
        state = [self.I, self.R, self.Psi]
        
        # Solve ODE
        result = solve_ivp(
            lambda t_, s: id14_derivatives(t_, s, self.params_14, Phi_avg, Psi_d_feedback),
            [self.t, self.t + self.dt],
            state,
            method='RK45'
        )
        
        # Update state
        self.I, self.R, self.Psi = result.y[:, -1]
        
        # 3. Evolve Environment (ID26) - The agent "digests" the logs
        self.Phi = id26_step_1d(self.Phi, self.Psi, self.params_14, self.params_26, self.dx, self.dt)
        
        # Increment time
        self.t += self.dt
        
    def get_state(self):
        """
        Translate physics variables into Agent capabilities.
        """
        return {
            "attention": self._classify_psi(self.Psi),
            "creativity": self._classify_i(self.I),
            "action_permission": self._classify_r(self.R),
            "raw": {
                "I": self.I,
                "R": self.R,
                "Psi": self.Psi,
                "Phi_avg": np.mean(self.Phi)
            }
        }
        
    def _classify_psi(self, psi):
        if psi > 0.4: return "HIGH_ALERT"  # Use GPT-4
        if psi > 0.2: return "AWAKE"       # Use GPT-3.5
        return "SLEEPING"                  # Use grep / cheap logic
        
    def _classify_i(self, i):
        if i > 0.8: return "INSPIRED"      # Generate new solutions
        if i > 0.4: return "THINKING"      # Planning
        return "ROUTINE"                   # Following script
        
    def _classify_r(self, r):
        if r > 0.3: return "WRITE_ACCESS"  # Can edit files/deploy
        if r > 0.1: return "READ_ONLY"     # Can read/verify
        return "OBSERVER"                  # Passive monitoring
