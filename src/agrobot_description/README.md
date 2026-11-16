# agrobot_description

URDF/xacro-beschrijving van de agrobots, inclusief de ros2_control-hardwareconfig (welke ODrive bij welk wiel hoort). Het opstarten gebeurt met [agrobot_bringup](../agrobot_bringup), de ODrive-plugin en -documentatie staan in [agrobot_odrive](../agrobot_odrive).

Er zijn twee robots:

| Robot | Aandrijving | Hoofdbestand |
|---|---|---|
| **Curio** | differential drive: 2 aangedreven wielen voor, 2 zwenkwielen achter | [urdf/agrobot_curio.urdf.xacro](urdf/agrobot_curio.urdf.xacro) |
| **ATD** | 4 aangedreven wielen, skid steering | [urdf/agrobot_atd.urdf.xacro](urdf/agrobot_atd.urdf.xacro) |

## Bestanden

| Bestand | Inhoud |
|---|---|
| [agrobot_curio.urdf.xacro](urdf/agrobot_curio.urdf.xacro) | Hoofdbestand Curio: voegt de onderdelen hieronder samen. |
| [agrobot_curio_description.urdf.xacro](urdf/agrobot_curio_description.urdf.xacro) | Macro `agrobot_curio`: links, joints en afmetingen (eenvoudige vormen: box en cilinders). |
| [agrobot_curio.materials.xacro](urdf/agrobot_curio.materials.xacro) | Kleuren voor RViz. |
| [agrobot_curio.ros2_control.xacro](urdf/agrobot_curio.ros2_control.xacro) | Macro `agrobot_curio_ros2_control`: hardware-plugin, node_id's en overbrenging. |
| [agrobot_atd.urdf.xacro](urdf/agrobot_atd.urdf.xacro) | Hoofdbestand ATD. |
| [agrobot_atd.urdf](urdf/agrobot_atd.urdf) | Links, joints, meshes en materialen, gegenereerd uit het CAD-model (`Agrobot_assem.STEP`, SolidWorks 2025). Gewone URDF, ondersteunt geen `prefix`. |
| [agrobot_atd.ros2_control.xacro](urdf/agrobot_atd.ros2_control.xacro) | Macro `agrobot_atd_ros2_control`: hardware-plugin, node_id's en overbrenging. |
| [meshes/](meshes/) | STL-bestanden van de ATD (chassis, accu, wielbehuizing, vier wielen). |

## Xacro-argumenten

Beide hoofdbestanden hebben dezelfde argumenten:

| Argument | Standaard | Betekenis |
|---|---|---|
| `use_mock_hardware` | `false` | `true`: `mock_components/GenericSystem` in plaats van de ODrives. |
| `prefix` | `""` | Voorvoegsel voor joint- en linknamen. Werkt bij de ATD alleen voor de ros2_control-joints, niet voor de links/joints uit `agrobot_atd.urdf`, dus gebruik het daar niet. |

Zelf genereren of controleren:

```bash
xacro $(ros2 pkg prefix agrobot_description)/share/agrobot_description/urdf/agrobot_curio.urdf.xacro use_mock_hardware:=true > /tmp/curio.urdf
check_urdf /tmp/curio.urdf
```

## Frames

REP-103: x vooruit, y links, z omhoog.

- `base_footprint`: root-link op de grond, onder het rotatiecentrum. Dit is het `base_frame_id` van de diff-drive-controller.
- `base_link`: op wielashoogte (`base_footprint` + wielstraal in z).
- Wielen draaien om de y-as van hun joint (`continuous`); positief = vooruit.

### Curio

- `left_wheel_joint` / `right_wheel_joint` op y = ±0,6675 m (spoorbreedte 1,335 m), wielstraal 0,1257 m.
- `base_link` ligt op het midden van de aangedreven as. Die as zit onder de voorrand van het chassis, dus het chassis (1,49 × 1,49 m, 0,62 m boven de grond) is naar achteren verschoven.
- Zwenkwielen `caster_rear_{left,right}_wheel` onder de achterhoeken (straal 0,13 m, 4.10/3.50-4-band). Ze zijn als `fixed` joint gemodelleerd: ze dienen alleen voor visualisatie en collisie.
- Massa's en traagheden zijn willekeurige waarden (geen simulatie).

### ATD

- `{front,rear}_{left,right}_wheel_joint` op x ≈ ±0,40 m en y = ±0,776 m (spoorbreedte 1,552 m), wielstraal 0,2032 m.
- Massa's zijn schattingen uit het CAD-model (aluminium 2700 kg/m³, accu 2000 kg/m³, wiel 8 kg). Pas aan naar gewogen waarden.

## ros2_control

Beide robots gebruiken de plugin `agrobot_odrive/GearedODriveHardwareInterface` op CAN-bus `can0`: de originele ODrive-plugin plus een overbrenging per joint (zie [agrobot_odrive](../agrobot_odrive/README.md)). Per joint:

- `node_id`: CAN-node-ID van de ODrive van dat wiel.
- `gear_ratio` = 50: 10 poolparen × 5:1 planetaire overbrenging; de ODrive is ingesteld op 1 poolpaar. ROS werkt daardoor in wielomwentelingen (rad, rad/s).

Koppeling node_id ↔ wiel:

| node_id | Motor | Curio | ATD |
|---|---|---|---|
| 1 | A | rechts (`right_wheel_joint`) | rechtsachter (`rear_right_wheel_joint`) |
| 2 | B | links (`left_wheel_joint`) | linksachter (`rear_left_wheel_joint`) |
| 3 | C | – | linksvoor (`front_left_wheel_joint`) |
| 4 | D | – | rechtsvoor (`front_right_wheel_joint`) |

De bijbehorende ODrive-configs staan in [agrobot_odrive/config/odrive/](../agrobot_odrive/config/odrive/); achtergrond in [ODRIVE.md](../agrobot_odrive/docs/ODRIVE.md).

Met `use_mock_hardware:=true` worden de interfaces (`velocity` als command, `position` en `velocity` als state) expliciet in de URDF gezet, omdat de mock-hardware ze niet zelf exporteert zoals de ODrive-plugin dat doet.

## Wijzigen

- Afmetingen die ook in de controllerconfig staan (`wheel_separation`, `wheel_radius` in [agrobot_bringup/config/](../agrobot_bringup/config/)) moeten daar ook worden aangepast.
- Een ander wiel of andere ODrive koppelen: pas `node_id` aan in het `*.ros2_control.xacro`-bestand van de robot.
- Nieuwe ATD-geometrie uit CAD: vervang [agrobot_atd.urdf](urdf/agrobot_atd.urdf) en de meshes, en houd de jointnamen gelijk (die gebruiken de ros2_control-xacro en de controllerconfig).
