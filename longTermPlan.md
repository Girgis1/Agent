# 🚀 Agent Unreal Game: Long-Term Plan Workshop
*Version 1.0 - Dynamic roadmap for iterative development*

## 🎯 Core Vision Refinement
**Elevator Pitch [LOCKED]**:
*"Chaotic physics-based resource puzzle where strength equals capacity, nothing respawns, and the world shredder is your only salvation."*

## 🔧 Phase 1: Foundation Testbed (Questions to Explore)
### Mechanics Validation
- [ ] **Testing Question**: What's the realistic lift-to-strength ratio?
- [ ] **Testing Question**: How does weight affect movement *feel* at different scales?
- [ ] **Testing Question**: What makes shredder placement *intuitive* vs *frustrating*?

### Technical Architecture
```
□ Custom physics mass tracking system
□ Speed-to-weight relationship curves
□ Shredder destruction procedural system
□ Weight-based interaction gates
```

## 📊 Character Variable System (0.1-100 → 10-10000 scale)

### Core Variables (Integer 10-10000)
```cpp
float Strength = 100.0f;          // Base 10-10000 
float Stamina = 100.0f;           // Energy pool
float Speed = 100.0f;             // Movement velocity
float JumpHeight = 100.0f;        // Vertical capacity
float Clumsiness = 100.0f;        // Stability threshold
float Recovery = 100.0f;          // Regeneration rate
```

### Interdependent Math Model
**Strength ↔ All Stats:**
```cpp
Speed = max(Strength * 0.5f, Speed);          // Strength baseline for speed
Stamina = max(Strength * 0.3f, Stamina);      // Endurance floor by strength
JumpHeight = (Strength * JumpInfluence 0.2f) + JumpHeight;
```

### Progressive Influence Loops
**Jump Cycles:**
```cpp
float jumpBonus = (TimeInAir * 0.001f) * JumpInfluence;  // Micro-strength gains
JumpHeight += jumpBonus;
Strength += jumpBonus * 0.1f;                            // Feedback loop
```

### Clumsiness & Recovery Dynamics
**Movement Climate:**
```cpp
float startTripChance = min(Clumsiness * Velocity, 85.0f);   // Caps at 85%
float stepHeightPenalty = StepHeight * 0.1f;                 // Elevation danger
float totalTripChance = startTripChance + stepHeightPenalty;
```

### Recovery Spreadsheet
```cpp
float exhaustionLevel = Stamina / MaxStamina;
RecoveryRate = min(10.0f, 100.0f - exhaustionLevel);          // Fast recovery when rest
// Faster recovery when exhaustion > 70%
```

### UI Scale References
```markdown
Human Readings:
- **Beginner**: 10 (clumsy, slow)
- **Athletic**: 50 (quality baseline)
- **Elite**: 100 (smooth, fast)
- **World Record**: 10000 (absolute precision)
```

### Environment Physics
**Surface Penalty:**
- **Flat ground**: Trip 0.1% (extremely low)
- **Uneven terrain**: Trip 25-50%  
- **Backwards movement**: Trip probability += Velocity * 0.02
- **Recovery time scales**: 1-10 seconds based on Stamina exhaustion
```

## 🎯 Core Concepts Workshop
### Resource Strategy (Your choice)
- **Single material chain**: Paper → Gears → Engines → Mega shredder  
- **Multiple material types**: Each with unique physics properties
- **Emergent materials**: Physics determines what drops from shredder

### World State Persistence
- **Checkpoint system**: Manual save at shredder locations
- **Progression gates**: Weight requirements unlock shredder access
- **State persistence**: Save all world object positions/masses

## 📋 Next Brainstorming Prompts  
**What would make the shredder placement:**
- Feel like solving a physics puzzle vs brute forcing?
- Reward creative engineering over grinding?
- Maintain tension even with powerful tools?

**Questions to workshop:**
1. **Minimum viable shredder** - simplest physics interaction?
2. **World scale** - stadium vs warehouse vs world?
3. **Progression pacing** - gradual vs exponential?

---
**Note to self**: This is Daniel's sandbox - I observe and suggest, never implement