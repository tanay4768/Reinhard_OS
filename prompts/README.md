# Reinhard OS overlay

Files to drop onto your existing clone of github.com/tanay4768/Reinhard_OS:

- `Makefile`                      fixed build (no space in the kernel name, header dependencies)
- `docs/STATUS.md`, `docs/ROADMAP.md`
- `scripts/rename-to-reinhard.sh` normalises Akira/old names to Reinhard
- `scripts/apply-overlay.sh`      does everything below in one go

    git clone https://github.com/tanay4768/Reinhard_OS && cd Reinhard_OS
    bash /path/to/reinhard-os-overlay/scripts/apply-overlay.sh .
    make run

This overlay does NOT contain the kernel sources (src/, include/, linker.ld, grub.cfg);
those come from your repository. The boot panic (KI-1 in docs/STATUS.md) still needs a
source fix in src/mm.
