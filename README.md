# bridge_rest 

[![Build and Publish Debian Package](https://github.com/robotkernel-hal/bridge_rest/actions/workflows/build-deb.yaml/badge.svg)](https://github.com/robotkernel-hal/bridge_rest/actions/workflows/build-deb.yaml)
[![License: LGPL-V3](https://img.shields.io/badge/license-LGPL--V3-green.svg)](LICENSE)
[![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)](#)
[![Debian](https://img.shields.io/badge/Debian-A81D33?logo=debian&logoColor=fff)](#)
[![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?logo=ubuntu&logoColor=white)](#)

**RESTful HATEOAS bridge for robotkernel-hal**

`bridge_rest` exposes the internal services of [robotkernel-hal](https://github.com/robotkernel-hal) as a RESTful web service using JSON payloads. It enables read and write access (e.g., CANopen, EtherCAT) via HTTP, facilitating integration with web-based frontends or remote systems.

This bridge follows REST and HATEOAS principles and is designed to be lightweight, real-time-friendly, and embeddable.

---

## ✨ Features

- 🔗 JSON-based REST API  
- 🌐 HATEOAS-compliant navigation of device trees  
- 🧩 Minimal dependencies  
- ⚙️ Compatible with PREEMPT-RT and embedded Linux  
- 🛠️ Modular and extendable  

---

## 🛠️ Usage Examples

### 🔍 Get API root:

```bash
curl http://localhost:8080/ap/v2.0
```

Response:

```json
{
  "slaves": "/slaves",
  "_links": {
    "self": { "href": "/" },
    "slaves": { "href": "/slaves" }
  }
}
```

---

## 📦 Build Instructions

Please make sure that the prerequisites are installed. These are:

    robotkernel

Then you should be able to build module_tty with:

```bash
git clone https://github.com/robotkernel-hal/module_tty.git
cd module_tty
./bootstrap.sh
autoreconf -i -f
mkdir build && cd build
../configure
make
sudo make install
```

---

## 🤝 Contributing

Contributions welcome! Please ensure:

- No compiler warnings
- Code follows robotkernel coding rules
- Tests included for new features

---

## 📄 License

Licensed under the **LGPL-V3 License**. See the [LICENSE](LICENSE) file.

---

**Robotkernel HAL Project** – powering real-time robotics infrastructure with modular, modern C++
