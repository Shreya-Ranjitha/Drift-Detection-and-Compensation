#include <reg51.h>

/* LCD Control Pins */
sbit RS = P1^0;
sbit RW = P1^1;
sbit EN = P1^2;

/* I2C Pins */
sbit SDA = P3^0;
sbit SCL = P3^1;

/* Global Offsets */
int offset1 = 0;
int offset2 = 0;
int offset3 = 0;

/* ---------------- Delay Functions ---------------- */

void delay_short()
{
    unsigned int i;
    for(i=0;i<500;i++);
}

void delay_long()
{
    unsigned int i,j;
    for(i=0;i<300;i++)
        for(j=0;j<200;j++);
}

/* ---------------- LCD Functions ---------------- */

void lcd_enable_pulse()
{
    EN = 1;
    delay_short();
    EN = 0;
}

void lcd_write_nibble(unsigned char data)
{
    P1 = (P1 & 0x0F) | (data & 0xF0);
    lcd_enable_pulse();
}

void lcd_command(unsigned char cmd)
{
    RS = 0;
    RW = 0;

    lcd_write_nibble(cmd);
    lcd_write_nibble(cmd << 4);

    delay_short();
}

void lcd_data(unsigned char data)
{
    RS = 1;
    RW = 0;

    lcd_write_nibble(data);
    lcd_write_nibble(data << 4);

    delay_short();
}

void lcd_print_string(char *str)
{
    while(*str)
        lcd_data(*str++);
}

void lcd_print_number(int num)
{
    if(num < 0)
    {
        lcd_data('-');
        num = -num;
    }

    lcd_data((num/100)%10 + '0');
    lcd_data((num/10)%10 + '0');
    lcd_data(num%10 + '0');
}

void lcd_initialize()
{
    delay_short();
    delay_short();

    lcd_command(0x02);
    lcd_command(0x28);
    lcd_command(0x0C);
    lcd_command(0x06);
    lcd_command(0x01);
}

/* ---------------- I2C Functions ---------------- */

void i2c_start()
{
    SDA = 1;
    SCL = 1;
    SDA = 0;
    SCL = 0;
}

void i2c_stop()
{
    SDA = 0;
    SCL = 1;
    SDA = 1;
}

void i2c_write(unsigned char data)
{
    unsigned char i;

    for(i=0;i<8;i++)
    {
        SDA = (data & 0x80);
        SCL = 1;
        SCL = 0;
        data <<= 1;
    }

    SDA = 1;
    SCL = 1;
    SCL = 0;
}

unsigned char i2c_read()
{
    unsigned char i, data = 0;

    SDA = 1;

    for(i=0;i<8;i++)
    {
        SCL = 1;
        data <<= 1;

        if(SDA)
            data |= 1;

        SCL = 0;
    }

    return data;
}

/* ---------------- MPU6050 Functions ---------------- */

void mpu_initialize(unsigned char address)
{
    i2c_start();
    i2c_write(address);
    i2c_write(0x6B);   // Power Management Register
    i2c_write(0x00);   // Wake up sensor
    i2c_stop();
}

int mpu_read_gyro_x(unsigned char address)
{
    unsigned char high, low;
    int value;

    i2c_start();
    i2c_write(address);
    i2c_write(0x43);   // Gyro X High Register

    i2c_start();
    i2c_write(address + 1);

    high = i2c_read();
    low  = i2c_read();

    i2c_stop();

    value = (high << 8) | low;

    return value;
}

/* ---------------- Calibration ---------------- */

void calibrate_sensors()
{
    int i;
    long sum1 = 0, sum2 = 0, sum3 = 0;

    for(i=0;i<20;i++)
    {
        sum1 += mpu_read_gyro_x(0xD0);
        sum2 += mpu_read_gyro_x(0xD2);
        sum3 += mpu_read_gyro_x(0xD0);   // demo reuse

        delay_short();
    }

    offset1 = sum1 / 20;
    offset2 = sum2 / 20;
    offset3 = sum3 / 20;
}

/* ---------------- Median Voting ---------------- */

int compute_median(int a, int b, int c)
{
    if((a > b && a < c) || (a < b && a > c))
        return a;
    else if((b > a && b < c) || (b < a && b > c))
        return b;
    else
        return c;
}

/* ---------------- Main Function ---------------- */

void main()
{
    int s1, s2, s3;
    int voted_value;

    lcd_initialize();

    mpu_initialize(0xD0);
    mpu_initialize(0xD2);

    calibrate_sensors();

    while(1)
    {
        /* Drift Compensation */
        s1 = mpu_read_gyro_x(0xD0) - offset1;
        s2 = mpu_read_gyro_x(0xD2) - offset2;
        s3 = mpu_read_gyro_x(0xD0) - offset3;

        /* Median Voting */
        voted_value = compute_median(s1, s2, s3);

        /* Display */
        lcd_command(0x80);
        lcd_print_string("S1:");
        lcd_print_number(s1);

        lcd_print_string(" S2:");
        lcd_print_number(s2);

        lcd_command(0xC0);
        lcd_print_string("S3:");
        lcd_print_number(s3);

        lcd_print_string(" V:");
        lcd_print_number(voted_value);

        delay_long();
    }
}
