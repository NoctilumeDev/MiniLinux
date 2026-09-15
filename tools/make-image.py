"""Make the small BIOS CD image used by M0."""

import sys
from pathlib import Path

import pycdlib


def main(project: Path, tool_root: Path) -> None:
    limine = tool_root / "limine-12.9.0" / "limine-binary"
    build = project / "build"
    iso = pycdlib.PyCdlib()
    iso.new(interchange_level=3, rock_ridge="1.09", joliet=3,
            vol_ident="MINILINUX_M0")
    iso.add_directory(iso_path="/BOOT", rr_name="boot", joliet_path="/boot")

    files = (
        (limine / "limine-bios-cd.bin", "/LIMCD.BIN;1",
         "limine-bios-cd.bin", "/limine-bios-cd.bin"),
        (limine / "limine-bios.sys", "/LIMINE.SYS;1",
         "limine-bios.sys", "/limine-bios.sys"),
        (project / "boot" / "limine.conf", "/LIMINE.CON;1",
         "limine.conf", "/limine.conf"),
        (build / "kernel.elf", "/BOOT/KERNEL.ELF;1",
         "kernel.elf", "/boot/kernel.elf"),
    )
    for source, iso_path, rr_name, joliet_path in files:
        iso.add_file(str(source), iso_path=iso_path, rr_name=rr_name,
                     joliet_path=joliet_path)

    iso.add_eltorito("/LIMCD.BIN;1", bootcatfile="/BOOT.CAT;1",
                     rr_bootcatname="boot.cat", joliet_bootcatfile="/boot.cat",
                     boot_load_size=4, boot_info_table=True,
                     media_name="noemul")
    output = tool_root / "images" / "minilinux-m0.iso"
    output.parent.mkdir(parents=True, exist_ok=True)
    iso.write(str(output))
    iso.close()
    # Limine's BIOS reader transfers whole sectors near the end of an image.
    # Leave spare readable sectors after the last file, as ISO tools commonly do.
    with output.open("ab") as image:
        image.write(bytes(1024 * 1024))
    print(output)


if __name__ == "__main__":
    main(Path(sys.argv[1]), Path(sys.argv[2]))
