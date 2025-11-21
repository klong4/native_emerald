# Training Guide - Pokemon Emerald RL

Complete guide to training reinforcement learning agents on Pokemon Emerald.

## Table of Contents
- [Training Basics](#training-basics)
- [Reward Function Design](#reward-function-design)
- [Hyperparameter Tuning](#hyperparameter-tuning)
- [Advanced Techniques](#advanced-techniques)
- [Troubleshooting](#troubleshooting)

## Training Basics

### Quick Training Run
```bash
python train_ppo.py --rom ../pokeemerald.gba --steps 50000
```

### Full Training Run
```bash
python train_ppo.py --rom ../pokeemerald.gba --steps 5000000
```

### Resume Training
```bash
# Load existing model and continue training
python train_ppo.py --rom ../pokeemerald.gba --steps 1000000 --resume ./models/ppo_emerald_10000_steps
```

## Reward Function Design

The reward function is THE most important part of RL training. Here's how to design good rewards:

### Current Reward Structure
```python
# emerald_env.py _calculate_reward()
reward = 0.0

# Major progress rewards
+ 1000 per badge earned
+ 5 for visiting new map
+ money_gained / 1000

# Penalties
- hp_lost * 0.1
- 0.01 per frame (time penalty)
```

### Reward Design Principles

1. **Sparse vs Dense Rewards**
   - Sparse: Big rewards for rare events (badges)
   - Dense: Small rewards for common events (exploration)
   - Best: Mix of both!

2. **Reward Scaling**
   - Badge: 1000 (very rare, important)
   - New map: 5 (common, minor progress)
   - HP loss: -0.1 per HP (continuous feedback)

3. **Shaped Rewards** (Advanced)
   ```python
   # Reward for getting closer to objective
   dist_to_gym = calculate_distance(player_pos, gym_pos)
   reward += -0.001 * dist_to_gym
   ```

### Examples of Good Rewards

**For Battle Training:**
```python
def _calculate_battle_reward(self):
    reward = 0.0
    
    # Damage dealt to opponent
    opp_hp_lost = prev_opp_hp - current_opp_hp
    reward += opp_hp_lost * 0.5
    
    # Own HP preserved
    own_hp_lost = prev_own_hp - current_own_hp
    reward -= own_hp_lost * 1.0
    
    # Win bonus
    if battle_won:
        reward += 100
        
    return reward
```

**For Exploration:**
```python
def _calculate_explore_reward(self):
    reward = 0.0
    
    # New tiles visited
    if current_tile not in visited_tiles:
        reward += 1.0
        visited_tiles.add(current_tile)
    
    # Item collected
    if item_count > prev_item_count:
        reward += 10.0
        
    return reward
```

## Hyperparameter Tuning

### PPO Hyperparameters

```python
model = PPO(
    'CnnPolicy',
    env,
    learning_rate=2.5e-4,    # How fast to learn
    n_steps=128,              # Steps before update
    batch_size=256,           # Batch size for training
    n_epochs=4,               # Training epochs per update
    gamma=0.99,               # Discount factor
    gae_lambda=0.95,          # GAE parameter
    clip_range=0.1,           # PPO clip range
    ent_coef=0.01,            # Entropy coefficient
)
```

### Tuning Guide

| Hyperparameter | Too Low | Good | Too High |
|----------------|---------|------|----------|
| `learning_rate` | Slow learning | Stable progress | Unstable, diverges |
| `n_steps` | High variance | Balanced | Slow updates |
| `batch_size` | Noisy gradients | Smooth | Memory issues |
| `gamma` | Short-sighted | Long-term planning | Slow learning |
| `ent_coef` | Exploits early | Explores well | Random behavior |

### Finding Good Hyperparameters

1. **Start with defaults**
2. **Train for 50k steps**
3. **Check tensorboard:**
   - Reward increasing? Good!
   - Reward flat? Adjust learning_rate
   - Reward spiky? Increase batch_size
4. **Iterate**

## Advanced Techniques

### 1. Frame Stacking
```python
from stable_baselines3.common.vec_env import VecFrameStack

env = DummyVecEnv([make_env(rom_path)])
env = VecFrameStack(env, n_stack=4)  # Stack last 4 frames
```

### 2. Reward Normalization
```python
from stable_baselines3.common.vec_env import VecNormalize

env = VecNormalize(env, norm_reward=True, norm_obs=False)
```

### 3. Curriculum Learning
```python
# Start with easier tasks, gradually increase difficulty
if badges_earned < 2:
    # Easy: Just explore
    reward += exploration_bonus
else:
    # Medium: Battle training
    reward += battle_performance
if badges_earned >= 4:
    # Hard: Gym challenges
    reward += gym_progress
```

### 4. Parallel Training
```python
from stable_baselines3.common.vec_env import SubprocVecEnv

# Train with 4 parallel environments
env = SubprocVecEnv([make_env(rom_path) for _ in range(4)])
```

### 5. Pretrained Models
```python
# Load pretrained model and fine-tune
model = PPO.load("pretrained_exploration_model")
model.set_env(env)
model.learn(total_timesteps=1000000)
```

## Monitoring Training

### Tensorboard Metrics

```bash
tensorboard --logdir ./logs
```

**Key Metrics:**
- `rollout/ep_rew_mean`: Average episode reward (should increase)
- `train/loss`: Training loss (should decrease)
- `train/policy_loss`: Policy network loss
- `train/value_loss`: Value network loss
- `train/entropy_loss`: Exploration entropy

### Custom Logging

```python
# Add custom metrics to tensorboard
model.learn(
    total_timesteps=1000000,
    callback=TensorboardCallback(
        custom_metrics=['badges', 'money', 'party_level']
    )
)
```

## Troubleshooting

### Agent Not Learning

**Symptoms**: Reward stays flat after 100k+ steps

**Solutions**:
1. Check reward function - is it ever positive?
2. Increase learning_rate (try 5e-4)
3. Reduce epsilon for exploration
4. Simplify initial task (just moving around)

### Agent Overfits

**Symptoms**: Great performance, then sudden collapse

**Solutions**:
1. Reduce learning_rate
2. Increase batch_size
3. Add more exploration (higher ent_coef)
4. Use multiple seeds

### Training Too Slow

**Solutions**:
1. Reduce observation size
2. Use frame skipping
3. Parallel environments (SubprocVecEnv)
4. GPU training (CUDA)
5. Reduce n_steps

### Out of Memory

**Solutions**:
1. Reduce batch_size
2. Reduce n_steps
3. Smaller CNN architecture
4. Close other programs

## Example Training Schedule

### Week 1: Exploration
```bash
# Goal: Agent learns to navigate
python train_ppo.py --steps 500000 --save_path ./models/explore
# Expected: Visits ~20 different maps
```

### Week 2: Battle Basics
```bash
# Goal: Agent learns to win battles
python train_ppo.py --steps 1000000 --save_path ./models/battle
# Expected: Wins 50%+ of early battles
```

### Week 3: First Gym
```bash
# Goal: Agent gets first badge
python train_ppo.py --steps 2000000 --save_path ./models/gym1
# Expected: Reaches and attempts first gym
```

### Week 4: Full Run
```bash
# Goal: Multiple badges
python train_ppo.py --steps 5000000 --save_path ./models/full_game
# Expected: 2-3 badges, solid game progression
```

## Tips for Success

1. **Start Simple**: Train on exploration first, then add complexity
2. **Log Everything**: Track badges, money, Pokemon levels
3. **Visualize**: Watch agent play to understand behavior
4. **Iterate**: Try many reward functions, keep what works
5. **Patience**: Good results take 1M+ steps
6. **Save Often**: Checkpoint every 10k steps
7. **Multiple Seeds**: Train 3+ agents to verify reproducibility

## Going Further

- **Imitation Learning**: Record human play, train agent to mimic
- **Hierarchical RL**: Separate high-level (strategy) and low-level (controls) policies
- **Multi-Agent**: Train multiple agents, keep best performers
- **Transfer Learning**: Train on one Pokemon game, transfer to another

Happy training! 🚀
