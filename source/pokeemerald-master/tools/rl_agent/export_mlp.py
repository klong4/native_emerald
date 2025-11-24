#!/usr/bin/env python3
"""Export a tiny MLP to a C header with fixed-point weights for embedding.

Usage:
  python export_mlp.py --out ../native_runner/mlp_model.h --input-dim 64 --hidden 64 --output-dim 8

You can also pass an NPZ file with arrays: W1,b1,W2,b2 matching dimensions.
"""
import argparse
import numpy as np
import os


def write_header(path, W1, b1, W2, b2, scale=256):
    num_input = W1.shape[1]
    num_hidden = W1.shape[0]
    num_output = W2.shape[0]
    with open(path, 'w') as f:
        f.write('// Auto-generated MLP model header\n')
        f.write('#ifndef MLP_MODEL_H\n')
        f.write('#define MLP_MODEL_H\n\n')
        f.write('#include <stdint.h>\n\n')
        f.write(f'#define MLP_SCALE {scale}\n')
        f.write(f'#define MLP_NUM_INPUT {num_input}\n')
        f.write(f'#define MLP_NUM_HIDDEN {num_hidden}\n')
        f.write(f'#define MLP_NUM_OUTPUT {num_output}\n\n')

        # quantize weights
        W1_q = np.rint(W1 * scale).astype(np.int16)
        b1_q = np.rint(b1 * scale).astype(np.int32)
        W2_q = np.rint(W2 * scale).astype(np.int16)
        b2_q = np.rint(b2 * scale).astype(np.int32)

        f.write('static const int16_t MLP_W1[] = {\n')
        for val in W1_q.flatten():
            f.write(f' {int(val)},')
        f.write('\n};\n\n')

        f.write('static const int32_t MLP_B1[] = {\n')
        for val in b1_q.flatten():
            f.write(f' {int(val)},')
        f.write('\n};\n\n')

        f.write('static const int16_t MLP_W2[] = {\n')
        for val in W2_q.flatten():
            f.write(f' {int(val)},')
        f.write('\n};\n\n')

        f.write('static const int32_t MLP_B2[] = {\n')
        for val in b2_q.flatten():
            f.write(f' {int(val)},')
        f.write('\n};\n\n')

        f.write('#endif // MLP_MODEL_H\n')


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--npz', help='NPZ file containing W1,b1,W2,b2', default=None)
    p.add_argument('--out', help='Output header path', required=True)
    p.add_argument('--input-dim', type=int, default=64)
    p.add_argument('--hidden', type=int, default=64)
    p.add_argument('--output-dim', type=int, default=8)
    p.add_argument('--scale', type=int, default=256)
    args = p.parse_args()

    if args.npz:
        data = np.load(args.npz)
        W1 = data['W1']
        b1 = data['b1']
        W2 = data['W2']
        b2 = data['b2']
    else:
        # generate a small random model (for testing)
        rng = np.random.RandomState(1234)
        W1 = rng.normal(scale=0.1, size=(args.hidden, args.input_dim)).astype(np.float32)
        b1 = np.zeros((args.hidden,), dtype=np.float32)
        W2 = rng.normal(scale=0.1, size=(args.output_dim, args.hidden)).astype(np.float32)
        b2 = np.zeros((args.output_dim,), dtype=np.float32)

    out_dir = os.path.dirname(args.out)
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir)

    write_header(args.out, W1, b1, W2, b2, scale=args.scale)
    print('Wrote', args.out)


if __name__ == '__main__':
    main()
