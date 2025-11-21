"""
Example RL training script for Pokemon Emerald
Uses Stable Baselines3 with PPO algorithm
"""

import sys
from pathlib import Path
import argparse

sys.path.insert(0, str(Path(__file__).parent))

from emerald_env import EmeraldEnv
from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import CheckpointCallback, EvalCallback
from stable_baselines3.common.vec_env import DummyVecEnv, VecFrameStack
import numpy as np


def make_env(rom_path):
    """Create Pokemon Emerald environment"""
    def _init():
        env = EmeraldEnv(rom_path, render_mode='rgb_array')
        return env
    return _init


def train_agent(
    rom_path: str,
    total_timesteps: int = 1_000_000,
    save_path: str = './models',
    log_path: str = './logs',
):
    """
    Train a PPO agent on Pokemon Emerald
    
    Args:
        rom_path: Path to pokeemerald.gba
        total_timesteps: Number of training steps
        save_path: Directory to save models
        log_path: Directory for tensorboard logs
    """
    
    # Create directories
    Path(save_path).mkdir(parents=True, exist_ok=True)
    Path(log_path).mkdir(parents=True, exist_ok=True)
    
    print("=== Pokemon Emerald RL Training ===")
    print(f"ROM: {rom_path}")
    print(f"Total timesteps: {total_timesteps:,}")
    print(f"Model save path: {save_path}")
    print(f"Tensorboard logs: {log_path}\n")
    
    # Create environment
    print("Creating environment...")
    env = DummyVecEnv([make_env(rom_path)])
    
    # Stack frames for temporal information
    env = VecFrameStack(env, n_stack=4)
    
    print(f"Environment created: {env}")
    print(f"Observation space: {env.observation_space}")
    print(f"Action space: {env.action_space}\n")
    
    # Create callbacks
    checkpoint_callback = CheckpointCallback(
        save_freq=10000,
        save_path=save_path,
        name_prefix='ppo_emerald'
    )
    
    # Create PPO agent
    print("Creating PPO agent...")
    model = PPO(
        'CnnPolicy',
        env,
        verbose=1,
        tensorboard_log=log_path,
        learning_rate=2.5e-4,
        n_steps=128,
        batch_size=256,
        n_epochs=4,
        gamma=0.99,
        gae_lambda=0.95,
        clip_range=0.1,
        ent_coef=0.01,
    )
    
    print(f"Agent created: {model}\n")
    
    # Train
    print(f"Starting training for {total_timesteps:,} timesteps...")
    print("Monitor training with: tensorboard --logdir " + log_path)
    print()
    
    try:
        model.learn(
            total_timesteps=total_timesteps,
            callback=checkpoint_callback,
            progress_bar=True
        )
    except KeyboardInterrupt:
        print("\n\nTraining interrupted by user")
    
    # Save final model
    final_path = f"{save_path}/ppo_emerald_final"
    model.save(final_path)
    print(f"\nFinal model saved to: {final_path}")
    
    return model


def test_agent(rom_path: str, model_path: str, episodes: int = 10):
    """
    Test a trained agent
    
    Args:
        rom_path: Path to pokeemerald.gba
        model_path: Path to saved model
        episodes: Number of episodes to run
    """
    
    print("=== Testing Trained Agent ===")
    print(f"Model: {model_path}")
    print(f"Episodes: {episodes}\n")
    
    # Load model
    model = PPO.load(model_path)
    
    # Create environment (with rendering)
    env = EmeraldEnv(rom_path, render_mode='human')
    
    # Run episodes
    episode_rewards = []
    
    for episode in range(episodes):
        obs, info = env.reset()
        episode_reward = 0
        done = False
        steps = 0
        
        print(f"\nEpisode {episode + 1}/{episodes}")
        
        while not done:
            action, _states = model.predict(obs, deterministic=True)
            obs, reward, terminated, truncated, info = env.step(action)
            episode_reward += reward
            steps += 1
            done = terminated or truncated
            
            if steps % 100 == 0:
                print(f"  Step {steps}, Reward: {episode_reward:.2f}")
        
        episode_rewards.append(episode_reward)
        print(f"  Episode finished: {steps} steps, total reward: {episode_reward:.2f}")
    
    env.close()
    
    # Statistics
    print(f"\n=== Test Results ===")
    print(f"Episodes: {episodes}")
    print(f"Mean reward: {np.mean(episode_rewards):.2f} ± {np.std(episode_rewards):.2f}")
    print(f"Min reward: {np.min(episode_rewards):.2f}")
    print(f"Max reward: {np.max(episode_rewards):.2f}")


def main():
    parser = argparse.ArgumentParser(description='Train or test RL agent on Pokemon Emerald')
    parser.add_argument('--rom', type=str, default='../../source/pokeemerald-master/pokeemerald.gba',
                        help='Path to pokeemerald.gba ROM')
    parser.add_argument('--mode', type=str, choices=['train', 'test'], default='train',
                        help='Mode: train or test')
    parser.add_argument('--steps', type=int, default=1_000_000,
                        help='Training timesteps')
    parser.add_argument('--model', type=str, default='./models/ppo_emerald_final',
                        help='Model path for testing')
    parser.add_argument('--episodes', type=int, default=10,
                        help='Test episodes')
    
    args = parser.parse_args()
    
    # Check ROM exists
    if not Path(args.rom).exists():
        print(f"ERROR: ROM not found: {args.rom}")
        sys.exit(1)
    
    if args.mode == 'train':
        train_agent(args.rom, total_timesteps=args.steps)
    else:
        test_agent(args.rom, args.model, episodes=args.episodes)


if __name__ == '__main__':
    main()
