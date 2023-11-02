#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_BusIO_Register.h>

#define I2C_SDA 1
#define I2C_SCL 2
#define I2C_ADDRESS 0x3f

TwoWire twi = TwoWire(0);
Adafruit_I2CDevice i2c_dev = 0;

Adafruit_BusIO_Register heater_reg = Adafruit_BusIO_Register(&i2c_dev, 0);
Adafruit_BusIO_Register vent_in_reg = Adafruit_BusIO_Register(&i2c_dev, 1);
Adafruit_BusIO_Register vent_out_reg = Adafruit_BusIO_Register(&i2c_dev, 2);
Adafruit_BusIO_Register ssrs_reg = Adafruit_BusIO_Register(&i2c_dev, 3);
Adafruit_BusIO_Register checksum_reg = Adafruit_BusIO_Register(&i2c_dev, 4);
Adafruit_BusIO_Register inputs_reg = Adafruit_BusIO_Register(&i2c_dev, 5);



bool verifyChecksum(MCState *data, uint8_t checksum) {
  uint8_t sum = data->heating;
  sum += data->vent_in;
  sum += data->vent_out;
  sum += data->ssrs;

  sum = ~sum;

  uint8_t in1 = data->inputs & 0x0f;
  uint8_t in2 = ~(data->inputs);
  in2 = in2 >> 4;

  return (sum == checksum) && (in1 == in2);
}

static int lastTwiSetup = 0;
bool setupTwi() {
  // Limit setup rate to once every second
  int time = millis();
  if (time - lastTwiSetup < 10000)
    return false;
  lastTwiSetup = time;
  Serial.println("Setting up I2C");

  if (!twi.begin(I2C_SDA, I2C_SCL, 100000))
    return false;
  i2c_dev = Adafruit_I2CDevice(I2C_ADDRESS, &twi);

  heater_reg = Adafruit_BusIO_Register(&i2c_dev, 0);
  vent_in_reg = Adafruit_BusIO_Register(&i2c_dev, 1);
  vent_out_reg = Adafruit_BusIO_Register(&i2c_dev, 2);
  ssrs_reg = Adafruit_BusIO_Register(&i2c_dev, 3);
  checksum_reg = Adafruit_BusIO_Register(&i2c_dev, 4);
  inputs_reg = Adafruit_BusIO_Register(&i2c_dev, 5);

  return i2c_dev.begin();
}

static int lastTwiUpdate = 0;
void handleTwi() {
  if (i2cError) {
    i2cError = !setupTwi();
    return;
  }

  int time = millis();
  if (time - lastTwiUpdate < 10)
    return;
  lastTwiUpdate = time;

  //Serial.println("Updating I2C");

  bool write_successfull = true;
  write_successfull &= heater_reg.write(&nextState.heating, 1);
  write_successfull &= vent_in_reg.write(&nextState.vent_in, 1);
  write_successfull &= vent_out_reg.write(&nextState.vent_out, 1);
  nextState.ssrs &= 0b00011111;  // Top 3 bits reserved for heaters
  write_successfull &= ssrs_reg.write(&nextState.ssrs, 1);

  MCState read_data = { 0, 0, 0, 0, 0 };
  bool read_successfull = true;
  read_successfull &= heater_reg.read(&read_data.heating);
  read_successfull &= vent_in_reg.read(&read_data.vent_in);
  read_successfull &= vent_out_reg.read(&read_data.vent_out);
  read_successfull &= ssrs_reg.read(&read_data.ssrs);
  uint8_t checksum = 0;
  read_successfull &= checksum_reg.read(&checksum, 1);
  read_successfull &= inputs_reg.read(&read_data.inputs);

  /*
  Serial.print("Read data: ");
  Serial.print(currentState.heating, HEX);
  Serial.print(" ");
  Serial.print(currentState.vent_in, HEX);
  Serial.print(" ");
  Serial.print(currentState.vent_out, HEX);
  Serial.print(" ");
  Serial.print(currentState.ssrs, HEX);
  Serial.print(" ");
  Serial.print(checksum, HEX);
  Serial.print(" ");
  Serial.println(currentState.inputs, HEX);
  */

  if (!read_successfull || !write_successfull) {
    Serial.println("Read or write error");
    i2cDirty = true;
    i2cError = !setupTwi();
    return;
  }

  if (!verifyChecksum(&read_data, checksum)) {
    Serial.println("Wrong checksum");
    i2cDirty = true;
    return;
  }

  if (currentState.heating != read_data.heating
      || currentState.vent_in != read_data.vent_in
      || currentState.vent_out != read_data.vent_out
      || currentState.ssrs != read_data.ssrs
      || currentState.inputs != read_data.inputs)
    guiDirty = true;

  currentState.heating = read_data.heating;
  currentState.vent_in = read_data.vent_in;
  currentState.vent_out = read_data.vent_out;
  currentState.ssrs = read_data.ssrs;
  currentState.inputs = read_data.inputs;

  i2cDirty = false;
  //Serial.println("I2C updated.");
}