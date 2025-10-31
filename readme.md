# Influence Analysis System

This project provides a comprehensive platform for analyzing influence propagation in social networks. It features a high-performance C++ backend for core computations, a Python (Flask) API server, and a dynamic JavaScript frontend for interactive 3D graph visualization.

The system allows users to perform influence maximization (finding key spreaders), influence minimization (finding key blockers), and various community detection analyses, all within an intuitive web-based interface.

## Features

* **Influence Maximization (MAX):** Identifies a set of `k` nodes (seed nodes) that will maximize the spread of influence under a chosen propagation model.
* **Influence Minimization (MIN):** Identifies a set of `b` nodes to block, minimizing the spread of influence from a given set of negative seeds.
* **Community Search (CS):** Detects dense and cohesive sub-communities within the network's influenced regions based on:
    * **(k,l)-core**: A directed graph-based community model.
    * **k-core**: An undirected graph-based community model.
    * **k-truss**: A cohesive structure where every edge is part of at least (k-2) triangles.
* **Interactive 3D Visualization:** Renders the network graph in 3D, allowing users to explore propagation paths, final influence states, and community structures.
* **Real-time Analysis:** An interactive mode enables users to click on nodes to select seeds or blockers and see the influence calculation update in real-time.
* **Dynamic Animations:** The system visualizes the step-by-step process of influence propagation and influence blocking.
* **Multiple Propagation Models:** Supports both Independent Cascade (IC) and Linear Threshold (LT) models.
* **Multiple Probability Models:** Supports various edge weight probability models (WC, TR, CO).

## Datasets

The system is configured to work with sample datasets, referenced by their `dataset_id` in the UI and API calls:

* `soc`: A social network dataset from Slashdot.
* `facebook`: A dataset of Facebook user connections.
* `vote`: A dataset of Wikipedia editor voting networks.

## Getting Started

Follow these steps to build and run the entire application (backend and frontend).

### Prerequisites

* A C++ compiler with C++17 support (e.g., `g++` or `clang++`)
* `cmake`
* `make`
* `uuid-dev` (for UUID generation in C++)
* Python 3.x
* Flask and Flask-CORS (`pip install Flask flask-cors`)

### 1. Backend Setup (C++ & Python API)

The backend consists of the C++ algorithm core and a Python Flask server that provides an API.

1.  **Navigate to the Project Root:**
    Open a terminal and `cd` into the main project directory. This directory should contain `build_backend.sh`, `py_api/`, and `design_front/`.

2.  **Compile the C++ Core:**
    Run the build script. This will compile the C++ code into a Python module (`.so` file).
    ```bash
    bash build_backend.sh
    ```
    If successful, this will create a `.so` file (e.g., `imm_calculator.so`) inside the `py_api` directory.

3.  **Start the Backend Server:**
    Once the compilation is complete, run the Flask API server. By default, it runs on `http://127.0.0.1:5019`.
    ```bash
    python py_api/app.py
    ```
    Keep this terminal running.

### 2. Frontend Setup (JavaScript Client)

The frontend is a static web application that communicates with the backend API.

1.  **Open a New Terminal:**
    Leave the backend server running in its terminal. Open a new terminal window or tab.

2.  **Navigate to the Frontend Directory:**
    From the same project root, navigate into the frontend directory.
    ```bash
    cd design_front
    ```

3.  **Start the Frontend Server:**
    We will use Python's built-in HTTP server to serve the frontend files on port `8021`.
    ```bash
    python -m http.server 8021
    ```

### 3. Access the Application

You can now access the running application in your web browser at:
**[http://localhost:8021](http://localhost:8021)**

---