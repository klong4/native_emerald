from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv
from gym_poke_env import PokeEnv


def main():
    env = DummyVecEnv([lambda: PokeEnv(host="127.0.0.1", port=9999)])
    model = PPO("CnnPolicy", env, verbose=1)
    # small test run
    model.learn(total_timesteps=10000)
    model.save("ppo_poke")


if __name__ == "__main__":
    main()
