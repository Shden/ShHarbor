
// these pins go to the operation module of the hood:
const int operationOn = 12;
const int operationLight = 13;
const int operationS1 = 9;
const int operationS2= 10;
const int operationS3 = 11;

// these pins go to the control shield of the hood:
const int controlOn = 7;
const int controlLight = 8;
const int controlS1 = 4;
const int controlS2 = 5;
const int controlS3 = 6;

void setup() 
{
  // initialize operation-side pins:
  pinMode(operationOn, OUTPUT);
  pinMode(operationLight,  OUTPUT);
  pinMode(operationS1, INPUT);
  pinMode(operationS2, INPUT);
  pinMode(operationS3, INPUT);

  // initialize control-side pins:
  pinMode(controlOn, INPUT);
  pinMode(controlLight, INPUT);
  pinMode(controlS1, OUTPUT);
  pinMode(controlS2, OUTPUT);
  pinMode(controlS3, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:

}
