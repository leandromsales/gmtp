

i=1
max=5
echo "Starting $max clients..."
echo "Command: ./client.py 192.168.0.2"

while [ $i -le $max ]
do
     echo "Running $iº client"
    ./client.py 192.168.0.2 >> /dev/null &
    i=`expr $i + 1`
done

echo "Clients are running..."
