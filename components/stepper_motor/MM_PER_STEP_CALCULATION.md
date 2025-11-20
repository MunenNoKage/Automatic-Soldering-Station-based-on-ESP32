### ⚙️ Steps Per Millimeter Calculation (3D Printer Axis)

This calculation determines the number of **microsteps** the motor needs to perform to move the carriage by **1 millimeter (mm)**. This value is often referred to as **`Steps/mm`** or **`E-steps`** for linear axes in 3D printer firmware (e.g., Marlin, Klipper).

---

#### 1. Define Known Variables

| Variable | Symbol | Value | Explanation |
| :--- | :--- | :--- | :--- |
| **Belt Pitch** | $P$ | $2 \text{ mm}$ | GT2 standard belt step. |
| **Pulley Teeth** | $Z$ | $16$ | Number of teeth on your GT2 pulley. |
| **Motor Steps/Rev** | $S$ | $200$ | Standard for NEMA 17 stepper motors. |
| **Microstepping** | $M$ | $4$ | Set on your driver (1/4 microstep). |

---

#### 2. Calculation Steps

**A. Calculate Belt Travel Per Revolution ($L_{\text{rev}}$):**
(The linear distance the belt moves for one full pulley rotation.)

$$L_{\text{rev}} = Z \cdot P$$
$$L_{\text{rev}} = 16 \cdot 2 \text{ mm} = \mathbf{32 \text{ mm}}$$

**B. Calculate Total Microsteps Per Revolution ($\text{Total}_{\mu S}$):**
(The total number of electronic pulses needed for one full motor rotation.)

$$\text{Total}_{\mu S} = S \cdot M$$
$$\text{Total}_{\mu S} = 200 \cdot 4 = \mathbf{800 \text{ microsteps}}$$

**C. Calculate Steps Per Millimeter ($\text{Steps/mm}$):**
(The final configuration value for your firmware.)

$$\text{Steps/mm} = \frac{\text{Total}_{\mu S}}{L_{\text{rev}}}$$
$$\text{Steps/mm} = \frac{800 \text{ microsteps}}{32 \text{ mm}} = \mathbf{25 \text{ steps/mm}}$$

---

#### 3. Single Microstep Movement

To determine the linear distance travelled per single microstep:

$$\text{Movement}_{\mu S} = \frac{1}{\text{Steps/mm}}$$
$$\text{Movement}_{\mu S} = \frac{1}{25 \text{ steps/mm}} = \mathbf{0.04 \text{ mm/microstep}}$$