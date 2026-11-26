import time


def compute(width, height, max_iterations):
    iterations = 0
    weighted = 0
    interior = 0
    for py in range(height):
        for px in range(width):
            x0 = (px / width) * 3.5 - 2.5
            y0 = (py / height) * 3.0 - 1.5
            x = y = 0.0
            iteration = 0
            while x * x + y * y <= 4.0 and iteration < max_iterations:
                xtemp = x * x - y * y + x0
                y = 2.0 * x * y + y0
                x = xtemp
                iteration += 1
            iterations += iteration
            weighted += iteration * (py * width + px + 1)
            if iteration == max_iterations:
                interior += 1
    return iterations, weighted, interior


if __name__ == "__main__":
    start = time.perf_counter_ns()
    iterations, weighted, interior = compute(800, 800, 100)
    end = time.perf_counter_ns()
    print(f"seconds={(end - start) / 1e9:.9f} iterations={iterations} "
          f"weighted={weighted} interior={interior}")
    expected = (11_613_930, 3_721_766_287_256, 94_440)
    result = (iterations, weighted, interior)
    raise SystemExit(0 if result == expected else 1)
