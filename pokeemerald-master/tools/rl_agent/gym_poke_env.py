import socket
import json
import base64
import numpy as np
import gym
from gym import spaces


class PokeEnv(gym.Env):
    """Gym wrapper that communicates with an emulator bridge over TCP.

    The bridge must implement a small JSON protocol. This class is intentionally
    minimal — adapt observation and action spaces for your chosen task.
    """

    def __init__(self, host="127.0.0.1", port=9999, obs_shape=(160, 240, 3)):
        super().__init__()
        self.host = host
        self.port = port
        self.obs_shape = obs_shape
        # Example action space: combinations of buttons (A,B,UP,DOWN,LEFT,RIGHT,START,SELECT)
        self.buttons = ["A", "B", "UP", "DOWN", "LEFT", "RIGHT", "START", "SELECT"]
        self.action_space = spaces.MultiBinary(len(self.buttons))
        # Observation is an image by default (uint8)
        self.observation_space = spaces.Box(0, 255, shape=self.obs_shape, dtype=np.uint8)
        self.sock = None

    def _connect(self):
        if self.sock:
            return
        self.sock = socket.create_connection((self.host, self.port), timeout=5)
        self.f = self.sock.makefile(mode="rwb")

    def _send(self, obj):
        data = json.dumps(obj).encode("utf-8") + b"\n"
        self.f.write(data)
        self.f.flush()
        line = self.f.readline()
        if not line:
            raise ConnectionError("Bridge closed connection")
        return json.loads(line.decode("utf-8"))

    def reset(self):
        self._connect()
        resp = self._send({"cmd": "reset"})
        obs = self._decode_obs(resp.get("obs"))
        return obs

    def step(self, action):
        # convert binary vector to list of pressed button names
        pressed = [b for i, b in enumerate(self.buttons) if int(action[i])]
        resp = self._send({"cmd": "step", "buttons": pressed})
        obs = self._decode_obs(resp.get("obs"))
        reward = float(resp.get("reward", 0.0))
        done = bool(resp.get("done", False))
        info = resp.get("info", {})
        return obs, reward, done, info

    def render(self, mode="human"):
        # Optionally implement
        pass

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None

    def _decode_obs(self, obs_field):
        if obs_field is None:
            # bridge didn't return an observation
            return np.zeros(self.obs_shape, dtype=np.uint8)
        # Expect base64-encoded PNG or raw bytes (prefer base64-encoded raw bytes)
        try:
            raw = base64.b64decode(obs_field)
            arr = np.frombuffer(raw, dtype=np.uint8)
            # User must adapt parsing depending on what the bridge sends
            return arr.reshape(self.obs_shape)
        except Exception:
            # Fallback: empty observation
            return np.zeros(self.obs_shape, dtype=np.uint8)


if __name__ == "__main__":
    # quick sanity: connect and reset
    env = PokeEnv()
    try:
        o = env.reset()
        print("reset obs shape:", o.shape)
    except Exception as e:
        print("Could not connect to bridge:", e)
