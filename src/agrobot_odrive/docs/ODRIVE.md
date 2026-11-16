# ODrive-configuratie agrobot

Dit document beschrijft de configuratie van de twee ODrives die de aangedreven wielen van de agrobot aansturen. Het legt ook uit waarom de configuratie op 13-11-2026 is aangepast. Een vergelijking met de config van de leverancier staat in [ODRIVE_CONFIG_VERGELIJKING.md](ODRIVE_CONFIG_VERGELIJKING.md).

## Hardware

Curio (de ODrives in dit document):

| Wiel       | Motor | CAN node_id | Serienummer    | Model                              | Firmware |
|------------|-------|-------------|----------------|------------------------------------|----------|
| Rechtsvoor | A     | 1           | 57630766608947 | ODrive Pro (hw 4.4, variant 58)   | 0.6.12   |
| Linksvoor  | B     | 2           | 57604996936243 | ODrive Pro (hw 4.4, variant 58)   | 0.6.12   |

ATD (4 wielen, skid steering, zelfde ODrive-config; zie [agrobot_atd.ros2_control.xacro](../../agrobot_description/urdf/agrobot_atd.ros2_control.xacro)):

| Wiel         | Motor | CAN node_id |
|--------------|-------|-------------|
| Rechtsachter | A     | 1           |
| Linksachter  | B     | 2           |
| Linksvoor    | C     | 3           |
| Rechtsvoor   | D     | 4           |

- CAN-bus: `can0`, 250 kbit/s.
- De koppeling tussen node_id en wiel staat in [agrobot_curio.ros2_control.xacro](../../agrobot_description/urdf/agrobot_curio.ros2_control.xacro). Op de robot gecontroleerd door alleen node 1 te laten draaien: dat is het rechterwiel.

## Probleem: wielen liepen weg bij snelheid 0

Zodra een ODrive in closed loop ging, draaide het wiel met ongeveer −11 tot −12 omw/s. Het setpoint via CAN was daarbij 0. Dat gebeurde zowel via ROS (`odrive_ros2_control`) als met rechtstreeks verstuurde CAN-commando's zonder ROS. Een CAN-opname liet zien dat ROS uitsluitend `Set_Input_Vel = 0` stuurde.

### Oorzaak

Op beide ODrives stond GPIO16 ingesteld als analoge ingang, gekoppeld aan het snelheidssetpoint:

```
config.gpio16_mode                    = 3 (ANALOG_IN)
config.gpio16_analog_mapping.endpoint = axis0.controller.input_vel
config.gpio16_analog_mapping.min/max  = -30 / +30 omw/s
deadband                              = 0,415 – 0,585
```

De ODrive schrijft de waarde van deze ingang continu naar `input_vel`. Daardoor overschreef hij elk setpoint dat via CAN binnenkwam. Op de pin stond ongeveer 32% van het bereik. Dat ligt buiten de deadband en komt neer op ongeveer −10,6 omw/s. De pin was waarschijnlijk niet aangesloten (zwevend), wat ook verklaart dat de waarde schommelde.

Daarnaast stond `axis0.config.startup_closed_loop_control` op `True`. De ODrives gingen bij het aanzetten dus direct in closed loop, en de wielen draaiden zonder dat ROS actief was.

## Wijzigingen (beide ODrives)

| Parameter                                 | Oud                      | Nieuw     |
|-------------------------------------------|--------------------------|-----------|
| `config.gpio16_analog_mapping.endpoint`   | `axis0.controller.input_vel` | `None` |
| `config.gpio16_mode`                      | `3` (ANALOG_IN)          | `17` (AUTO, dezelfde stand als de andere GPIO's) |
| `axis0.config.startup_closed_loop_control`| `True`                   | `False`   |

Opgeslagen met `save_configuration()` en daarna na de herstart opnieuw uitgelezen.

Controle: per ODrive 3 s in closed loop gezet met setpoint 0 via CAN. Het wiel bleef stil staan: gemiddeld < 0,01 omw/s, met pieken tot ongeveer 0,5 omw/s door encoderruis. Er waren geen fouten.

ROS (`odrive_ros2_control`) zet de ODrives zelf in closed loop zodra de controller wordt geactiveerd. Daarvoor is `startup_closed_loop_control` niet nodig.

## Snelheidslimiet

`axis0.controller.config.vel_limit` is op beide ODrives verhoogd van `30` naar `100` omw/s (opgeslagen). Met de overbrenging van 50 (zie hieronder) komt dat neer op 1,58 m/s aan het wiel. De motoren halen bij een busspanning van ongeveer 54 V maximaal ongeveer 84 omw/s (≈ 1,33 m/s). Gemeten van de grond: rechts 84,5, links 83,4 omw/s. Daarom staat het maximum in ROS op 1,2 m/s (76 omw/s), zodat de regeling ruimte houdt.

## Koppelconstante en spinout

`axis0.config.motor.torque_constant` stond op de standaardwaarde `1.0`. Daardoor overschatte de ODrive het mechanische vermogen ongeveer 11 keer, en ging de spinout-detectie (`SPINOUT_DETECTED`, 0x4000000) vals af, vooral rechts.

Gemeten via de tegen-EMK (wiel van de grond, 19 en 37 omw/s, linkerwiel, `Vq = mod_q × ⅔ × Vbus`): **`torque_constant = 0.0875` Nm/A** in ODrive-eenheden (1 poolpaar). Dat komt overeen met een KV van de rotor van ongeveer 9,5 rpm/V. Let op: `foc.Vq_setpoint` is in fw 0.6 niet de toegepaste spanning.

`vel_gain` en `vel_integrator_gain` zijn in Nm per (omw/s). Ze zijn daarom met dezelfde factor meegeschaald (1,0 → 0,0875), zodat de regelaar per snelheidsfout dezelfde stroom geeft als voorheen. Zonder dat schalen was de regeling 11 keer te agressief en vielen beide assen direct uit met spinout.

Opgeslagen op beide ODrives (2026-11-13). Rijtest van de grond: geen uitval. Wel loopt rechts duidelijk onrustiger dan links (Iq ±3 A tegenover ±1 A bij 19 omw/s). Dat moet nog onderzocht worden (encoder of mechanisch).

Na de aanpassing van Kt bleven op de grond beide wielen af en toe uitvallen met `SPINOUT_DETECTED`, telkens bij **omkeren van de draairichting** (rijden of sturen). Uit een CAN-opname bleek dat dit een vals alarm is: het wiel remt dan met 13–24 A bij lage snelheid. Daardoor is het mechanische vermogen negatief, maar de koperverliezen (1,5·R·I² ≈ 100–290 W) maken het elektrische vermogen positief. Met de standaarddrempel van +10 W voldoet dat al aan het spinout-criterium. De encoders liepen in de opname vloeiend door.

Daarom is `axis0.controller.config.spinout_electrical_power_threshold` verhoogd van `10` naar **`500`** W, op beide ODrives (opgeslagen 2026-11-13). Dat ligt boven het maximale koperverlies bij `current_soft_max` = 30 A (≈ 460 W). `spinout_mechanical_power_threshold` blijft `-10`.

## Watchdog

Op beide ODrives staat `axis0.config.enable_watchdog = True` met `watchdog_timeout = 1.0` s (opgeslagen 2026-11-13). `odrive_ros2_control` stuurt elke cyclus (10 Hz) een `Set_Input_Vel`, en dat voedt de watchdog. Krijgt een ODrive 1 s lang niets, bijvoorbeeld omdat ROS crasht, de Pi hangt of de CAN-verbinding wegvalt, dan schakelt hij zelf naar IDLE met `WATCHDOG_TIMER_EXPIRED` (0x1000000).

Getest door `ros2_control_node` te pauzeren: beide assen gingen naar IDLE. Let op:
- Zolang ROS niet draait, staat `WATCHDOG_TIMER_EXPIRED` als actieve fout. Bij het activeren van de controller verdwijnt die vanzelf.
- ROS merkt een watchdog-uitval niet op. De controller blijft "active" terwijl de wielen in IDLE staan. Herstel: `ros2 control switch_controllers --deactivate agrobot` en daarna `--activate agrobot`.

## Overige relevante instellingen

Gelijk op beide ODrives:

```
axis0.config.motor.direction                 = -1.0
axis0.controller.config.control_mode         = 2 (VELOCITY_CONTROL)
axis0.controller.config.input_mode           = 1 (PASSTHROUGH)  (leverancier: 2, zie ODRIVE_CONFIG_VERGELIJKING.md)
axis0.controller.config.vel_gain             = 0.0875   (was 1.0, zie Koppelconstante)
axis0.controller.config.vel_integrator_gain  = 0.0875   (was 1.0, zie Koppelconstante)
axis0.controller.config.pos_gain             = 20.0
axis0.commutation_mapper.config.offset_valid = True
axis0.config.enable_watchdog                 = True     (was False, zie Watchdog)
axis0.config.watchdog_timeout                = 1.0
```

## Overbrenging motor → wiel

De wielen zijn Quinder 48V 8-inch naafmotoren met geïntegreerde encoder (RS-485, AMT21-compatibel, `RS485_ENCODER0`). De encoder is absoluut binnen één poolpaar. Daarom is de ODrive ingesteld op `pole_pairs = 1`, en is één "omwenteling" voor de ODrive één elektrische cyclus.

- Motor: 10 poolparen, dus 1 ODrive-omwenteling = 1/10 rotoromwenteling.
- Planetaire reductie: 5:1.
- **Totaal: 50 ODrive-omwentelingen per wielomwenteling.**

De omrekening gebeurt in de plugin `agrobot_odrive/GearedODriveHardwareInterface` (zie [README.md](../README.md)), via de jointparameter `gear_ratio` = 50 in [agrobot_curio.ros2_control.xacro](../../agrobot_description/urdf/agrobot_curio.ros2_control.xacro). ROS werkt dus in wielomwentelingen (rad, rad/s). `vel_limit`, de gains en alle waarden die je in `odrivetool` ziet zijn in ODrive-omwentelingen (motorzijde).

### Waarom niet op de ODrive zelf (`pos_vel_mapper.config.scale`)

Firmware 0.6 kan de overbrenging ook zelf verrekenen met `axis0.pos_vel_mapper.config.scale = 1/50`. Dat is op 16-11-2026 geprobeerd (niet opgeslagen, wielen in IDLE) en weer teruggedraaid. Bij stilstand bleek de ruis op `vel_estimate` in absolute getallen even groot te blijven (std ≈ 0,12 omw/s bij scale 1 én bij scale 0,02). In wielomwentelingen betekent dat 50× meer ruis, omgerekend ≈ 6 motoromw/s. Met de bijbehorende `vel_gain` van 4,375 zou dat ongeveer 6 A stroomruis geven in plaats van ≈ 0,1 A. De oorzaak in de firmware is niet uitgezocht. Daarom blijft de ODrive in motoreenheden werken en rekent ROS om.

## Bijbehorende ROS-instellingen

In [agrobot_curio_controllers.yaml](../../agrobot_bringup/config/agrobot_curio_controllers.yaml):

- `left_wheel_radius_multiplier`: van `-1.0` naar `1.0`. Bij beide ODrives betekent een negatieve motorsnelheid dat het wiel achteruit draait (op de robot gecontroleerd). Positief is dus voor beide wielen vooruit.
- `wheel_separation: 1.335` en `wheel_radius: 0.1257`, overeenkomstig de URDF.
- `linear.x.max_velocity: 1.2` / `min_velocity: -1.2`. De URDF-joint-limiet (`max_wheel_velocity`) komt hiermee overeen.

Getest van de grond: 0,5 m/s gaf gemiddeld 0,632 / 0,631 omw/s gemeten aan de motor (setpoint 0,633). Dat was nog zonder `gear_ratio`. 1,5 m/s gaf een setpoint van 94,96 omw/s, waarbij de motoren steken op ongeveer 84 omw/s, zie Snelheidslimiet.

## Noodstop via CAN

Als ROS stopt, zet dat de ODrives niet altijd terug naar idle. Beide assen direct naar IDLE zetten (`Set_Axis_State`, state 1):

```bash
cansend can0 027#01000000   # node 1, rechts
cansend can0 047#01000000   # node 2, links
```

## Config uitlezen of wijzigen via USB

`odrivetool` staat in een aparte venv, zodat de systeem-Python en ROS onaangetast blijven:

```bash
# eenmalig (al gedaan op deze Pi)
sudo apt install -y python3.12-venv
python3 -m venv ~/odrive-venv && ~/odrive-venv/bin/pip install odrive
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="1209", ATTR{idProduct}=="0d3[0-9]", MODE="0666"' | sudo tee /etc/udev/rules.d/91-odrive.rules
sudo udevadm control --reload-rules && sudo udevadm trigger

# gebruik: PYTHONPATH leeg, anders botsen de ROS-paden uit het shellprofiel met de venv
env -u PYTHONPATH ~/odrive-venv/bin/odrivetool
```

Sluit één ODrive tegelijk aan via USB en controleer `serial_number` en `axis0.config.can.node_id` voordat je iets wijzigt.

## Config-back-ups

In [config/odrive/](../config/odrive/) staan:

- [odrive_motor1.json](../config/odrive/odrive_motor1.json): Curio rechtsvoor, serienummer `346A355F3233`
- [odrive_motor2.json](../config/odrive/odrive_motor2.json): Curio linksvoor, serienummer `346435613233`
- `odrive_motorA.json` t/m `odrive_motorD.json`: sjablonen voor een nieuwe ODrive, zie hieronder
- [odrive_config_leverancier.json](../config/odrive/odrive_config_leverancier.json): de config zoals de leverancier die meeleverde

De node-bestanden zijn op 16-11-2026 via CAN gemaakt met `odrivetool backup-config`, met de waarden die in dit document staan. Maak na elke wijziging opnieuw een back-up (dat kan terwijl de bringup-service draait; uitlezen verandert niets):

```bash
env -u PYTHONPATH ~/odrive-venv/bin/odrivetool --can can0 --no-usb -s 346A355F3233 backup-config odrive_motor1.json
env -u PYTHONPATH ~/odrive-venv/bin/odrivetool --can can0 --no-usb -s 346435613233 backup-config odrive_motor2.json
```

Terugzetten gaat met `restore-config` (zelfde opties). Dat schrijft de config en slaat hem op. Doe dat alleen met de bringup-service gestopt en controleer daarna `axis0.config.can.node_id`.

### Sjablonen voor nieuwe ODrives (`odrive_motorX.json`)

| Bestand | node_id | Curio | ATD |
|---|---|---|---|
| [odrive_motorA.json](../config/odrive/odrive_motorA.json) | 1 | rechtsvoor | rechtsachter |
| [odrive_motorB.json](../config/odrive/odrive_motorB.json) | 2 | linksvoor | linksachter |
| [odrive_motorC.json](../config/odrive/odrive_motorC.json) | 3 | – | linksvoor |
| [odrive_motorD.json](../config/odrive/odrive_motorD.json) | 4 | – | rechtsvoor |

Gemaakt op 16-11-2026 uit [odrive_motor1.json](../config/odrive/odrive_motor1.json). Alleen `axis0.config.can.node_id` verschilt per bestand. De kalibratiewaarden horen bij één bepaalde motor. Daarom staan ze niet in de sjablonen en zijn ze als ongeldig gemarkeerd:

- `axis0.commutation_mapper.config.offset` (`offset_valid = false`)
- `axis0.config.motor.phase_resistance` (`phase_resistance_valid = false`)
- `axis0.config.motor.phase_inductance` (`phase_inductance_valid = false`)

Daardoor weigert de ODrive closed loop totdat hij gekalibreerd is. Een verkeerde commutatie-offset kan het wiel laten weglopen, zie [Probleem: wielen liepen weg bij snelheid 0](#probleem-wielen-liepen-weg-bij-snelheid-0).

Een nieuwe ODrive instellen (één ODrive tegelijk op de bus of via USB, bringup-service gestopt, wiel van de grond):

1. `odrivetool restore-config odrive_motorX.json`
2. Kalibreren: `axis0.requested_state = AxisState.FULL_CALIBRATION_SEQUENCE` (3). Het wiel draait daarbij even.
3. Controleer dat `phase_resistance_valid`, `phase_inductance_valid` en `commutation_mapper.config.offset_valid` nu `True` zijn, en dat er geen fouten zijn.
4. `save_configuration()`, en maak daarna een back-up met `backup-config`.
5. Controleer de draairichting: vooruit moet een positieve snelheid geven (de config heeft `axis0.config.motor.direction = -1`, net als bij de Curio). Zo niet, pas dan `motor.direction` aan en kalibreer opnieuw.
