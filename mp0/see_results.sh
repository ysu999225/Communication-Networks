git checkout _grades -q
git pull -q

echo -e '\n-----------Contents of results.txt-----------------\n'
cat results.txt
echo -e '\n-----------End of results.txt----------------------\n'

git checkout main -q
