# 🛠️ DOT World: Physics Systems Bible

## 🎯 Core Game Loop Architecture
```
Resource Tiers (Weight-Based) → 3x Lever Mechanics → Accelerated Progression
```

## 💪 Progressive Strength System
### Primary Scaling
- **Base capacity**: 10 units
- **Linear progression**: +1 strength per 50kg lifted total
- **Soft cap**: Non-linear decay after 100kg capacity
- **Hard cap**: 1000kg max lift (world-ending shredders)

### Movement Scaling
- **Running speed decay**: 100% (0kg) → 50% (500kg max)
- **Acceleration curves**: Linear burden vs exponential ability gain

## 🏗️ Shredder Physics Engine
### Object Classification
- **Tier 1**: 1-10kg → Paper shredders (basic tools)
- **Tier 2**: 100-500kg → Engine shredders (mechanical arms)  
- **Tier 3**: 1000kg+ → Vehicle shredders (resource pipelines)

### Physics Multipliers
- **Fulcrum system**: 3x leverage optimum ratio
- **Block-and-tackle**: 10x mechanical advantage
- **Vehicle winch**: 100x power multiplier (battery drain)

## ⚖️ Resource Scarcity Mathematics
### Shredder Yields
- **Paper**: 1 plastic unit per 5kg
- **Engines**: 1 gear unit per 100kg
- **Vehicles**: 1 engine unit per 1000kg (complex composites)

### Resource Chains
```
Plastic(1) → Tools → Increased_carry_capacity → 
Gears(1) → Mechanical_arms → 10x_leverage → 
Engines(1) → Resource_pipelines → 100x_efficiency → 
Final_shredder
```

## 🔧 Implementation Priority
### Phase 1: Core Systems (Week 1)
- [ ] Custom physics object base class
- [ ] Weight-based strength tracking
- [ ] Shredder destruction procedural system

### Phase 2: Mechanical Progression (Week 2)
- [ ] Lever physics simulation
- [ ] Vehicle battery drain mechanics
- [ ] Resource pipeline automation

### Phase 3: Resource Economy (Week 3)
- [ ] Progression gate unlocking
- [ ] Permadeath resource persistence
- [ ] World state serialization

## 📊 Balance Formulas
### Strength -> Capacity
```csharp
float capacity = 10.0f + (cumulativeMassLifted * 0.02f) - pow(cumulativeMassLifted, 1.5f) * 0.001f;
```

### Weight -> Movement Speed
```csharp
float speedMultiplier = 1.0f - (currentWeight / maxWeight) * 0.7f;
```

---
## 🚨 Design Decisions Locked
- **No respawn** = permanent world state
- **No inventory reset** = devoted to single save
- **Physics above all** = realistic mass/torque calculations