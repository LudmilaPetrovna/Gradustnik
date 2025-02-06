open(dd,$ARGV[0]);
read(dd,$file,-s(dd));
close(dd);

$file=~s/\r//gs;
$file=~s/\n[ \t]*\n\s+/\n\n/g;
$file=~s/\n[ \t]+/\n/g;
$file=~s/(\n\/\/)\s+/$1/g; # remove spaces in comment
$file=~s/(\([^\)]*\))\s*\{\s*/$1\{\n/gs;
$file=~s/(\n[^\(\)\[\]\{\}=\n]+?)\s*=\s*/$1=/gs;

%strings=();
$file=~s/(\"(\\\"|\\|[^\\\"\n])+?\")/$key="RND".rand().rand().rand().rand().rand();$strings{$key}=$1;$key/egs;

@lines=split(/\n/,$file);

$indent=0;
foreach $line(@lines){
$uncommented=$line;
$uncommented=~s/\/\/.*//s;
$open=($uncommented=~tr/({[/({[/);
$close=($uncommented=~tr/)}]/)}]/);
$addon=0;
if($open==$close && $line=~/\}\selse\s\{/){
$addon=-1;
}
if($close>$open){
$indent-=$close-$open;
if($indent<0){
$indent=0;
}
}

$out.="".("\t" x ($indent+$addon)).$line."\n";
if($open>$close){
$indent+=$open-$close;
}
}

foreach(keys %strings){
$out=~s/$_/$strings{$_}/gs;
}

#print $out;
open(oo,">".$ARGV[0]);print oo $out;close(oo);
