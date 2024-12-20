# ProcProtect
a project by Nishanth Nagendran, Pratheek Dhananjaya, Dhanush Bommavaram <br>

[report](https://1drv.ms/b/s!AmvH0VCG4AMojdUDNk06RMHTcZdZ8A?e=gpNbIp)

The program only works on linux based systems. <br>

Running the program:
1. run the following command,
```
bash setup.sh
```
2. pull the required model from ollama using,
```
ollama pull <model_name>
```
3. change the model name and context length of the model as required in GetData.c++ file.
4. compile GetData.c++ using the following command,
```
g++ -o GetData GetData.c++ -lcurl
```
5. run,
```
ollama serve
```
6. run GetData with root access,
```
sudo ./GetData
```
