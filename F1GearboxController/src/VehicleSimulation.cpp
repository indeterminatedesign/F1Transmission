#include <VehicleSimulation.h>

const double gearRatios[] =
    {0,
     2.0,
     1.57,
     1.25,
     1.0,
     0.93,
     0.86,
     0.73};
;

const double finalDriveCoefficent = 3.85;
const double totalDragProduct = .3; //Not an actual drag coefficent, just use to simulate drag increasing with speed and acts as product of all the other factors
const double totalMassConstant = 25;
const double powertrainFriction = 10;
const int idleRPM = 450;
const int revLimiter = 1200;
const int computeInterval = 100000; //Time in microseconds between recalculating a target rpm

const int enginePowerBins[15][2] = {
    {0, 0},
    {100, 0},
    {200, 0},
    {300, 100},
    {400, 250},
    {500, 300},
    {600, 350},
    {700, 575},
    {800, 625},
    {900, 775},
    {1000, 850},
    {1100, 930},
    {1200, 935},
    {1300, 850},
    {1400, 825}};

unsigned long previousSimulateMicros = 0;
int previousGear;
int newTargetRPM = idleRPM;

VehicleSimulation::~VehicleSimulation()
{
}

int VehicleSimulation::Simulate(float percentThrottle, int inputLayRPM, int currentGear)
{
    Serial.println("Entered Simulate");

    unsigned long currentMicros = micros();
    unsigned long dt = currentMicros - previousSimulateMicros; //Time elapsed since the vehicle was last simulated

    if (dt > computeInterval && inputLayRPM > 1)
    {
        if (currentGear > 0)
        {
            double effectiveGearRatio = gearRatios[currentGear] * finalDriveCoefficent;
            //final drive takes into account tire diameter
            long vehicleSpeed = inputLayRPM / effectiveGearRatio;

            //Effectively adding a floor to the percent throttle
            double effectivePercentThrottle = constrain(percentThrottle, 1, 100);

            //Force available at the current RPM assuming full throttle
            double totalAvailableCurrentForce = ComputeEngineForce(inputLayRPM);

            //Current power at the wheels based on the throttle position & gear ratio
            double currentEffectiveForce = 60 * effectivePercentThrottle * totalAvailableCurrentForce * effectiveGearRatio;
            double totalPowerTrainFriction = powertrainFriction * inputLayRPM;

            Serial.print("currentEffectiveForce ");
            Serial.println(currentEffectiveForce);
            Serial.print("totalPowerTrainFriction ");
            Serial.println(totalPowerTrainFriction);

            //A = F/M Thus engine power/some mass constant
            // Force of engine at the wheels is dependent on gear ratio
            //Net Decel being experienced based on speed (product of all drag area and coeficients) * vehicleSpeed^2
            double netAcceleration = .0003 * ((currentEffectiveForce / totalMassConstant) - totalPowerTrainFriction - (totalDragProduct * pow(vehicleSpeed, 2.5)));
            Serial.print("Net Acceleration: ");
            Serial.println(netAcceleration);


            //New target rpm is based on current rpm + delta T * netacceleration, based on V = V0 + AdT
            newTargetRPM = newTargetRPM + dt / 1000000.00 * netAcceleration;

            previousSimulateMicros = currentMicros;
            // Return newly Calculated target RPM
        }
        else
        {
            newTargetRPM = map(percentThrottle, 0, 100, idleRPM, revLimiter);
        }
        //Constrain RPM between idle and rev limiter
    }
    newTargetRPM = constrain(newTargetRPM, idleRPM, revLimiter);

    previousGear = currentGear;

    return newTargetRPM;
}

float VehicleSimulation::ComputeEngineForce(int layshaftRPM)
{
    //Interpolate the current engine power based on RPMs between two bins
    int lowerBinIndex = 0;
    int upperBinIndex = 0;
    for (int i = 0; i < 15; i++)
    {
        if (enginePowerBins[i][0] >= layshaftRPM)
        {
            lowerBinIndex = i - 1;
            upperBinIndex = i;
            break;
        }
    }

    //Use map function to linear interpolate the power between the two bins
    return map(layshaftRPM, enginePowerBins[lowerBinIndex][0], enginePowerBins[upperBinIndex][0], enginePowerBins[lowerBinIndex][1], enginePowerBins[upperBinIndex][1]);
}

//Cut Throttle based on PWM value
float VehicleSimulation::ThrottleCut()
{
    return 0.9;
}

int VehicleSimulation::RevMatch(int currentGear, int newGear, int layRPM)
{
    //Account for neutral which up or down has no rev match
    if (currentGear == 0 || newGear == 0)
    {
        return 1;
    }
    else
    {
        newTargetRPM = layRPM * gearRatios[newGear] / gearRatios[currentGear];
        return newTargetRPM;
    }
}