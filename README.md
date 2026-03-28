# gamer_time
Vulkan SDL3 + llamacpp game engine


Currently uses (Qwen3-VL-4B-Instruct-Q4_K_M.gguf)[https://huggingface.co/Qwen/Qwen3-VL-4B-Instruct-GGUF]

expects this model in ../model/Qwen3-VL-4B-Instruct-Q4_K_M.gguf
i.e.
Parent Directory/
├── model/
│   └── Qwen3-VL-4B-Instruct-Q4_K_M.gguf
└── gamer_time/
    └── SRC

# build
```
cmake -S . -B build -G Ninja

cd build
ninja run
```

