How to create a new library part
--------------------------------

The libraries are stored in an XML based format, called \*.oreglib, and are
installed in `<prefix>/share/oregano/libraries`.

The easiest way to describe how to create a part is to look at one from the
default library, the resistor. In each library, there is first a `<symbols>` tag.
This tag contains all the symbols used for the parts in the library. Take a look
at the resistor symbol:
```
    <ogo:symbol>
      <ogo:name>resistor</ogo:name>
      <ogo:objects>
        <ogo:line>(0 10)(10 10)(11 10)(13 6)(16 14)(19 6)(22 14)(25 6)(28 14)(30 10)(40 10)</ogo:line>
      </ogo:objects>
      <ogo:connections>
        <ogo:connection>(0 10)</ogo:connection>
        <ogo:connection>(40 10)</ogo:connection>
      </ogo:connections>
    </ogo:symbol>
```
It is built with a polyline, where each (x y) pair is a point on the line. So
far so good. Now lets take a look at the `<parts>` section, containing all the
parts that are shown in the part browser. The resistor definition looks like
this:
```
    <ogo:part>
      <ogo:name>Resistor</ogo:name>
      <ogo:symbol>resistor</ogo:symbol>
      <ogo:description>Resistor</ogo:description>
      <ogo:properties>
        <ogo:property>
          <ogo:name>Refdes</ogo:name>
          <ogo:value>R</ogo:value>
        </ogo:property>
        <ogo:property>
          <ogo:name>Res</ogo:name>
          <ogo:value>1k</ogo:value>
        </ogo:property>
        <ogo:property>
          <ogo:name>Template</ogo:name>
          <ogo:value>R_@refdes %1 %2 @res</ogo:value>
        </ogo:property>
      </ogo:properties>
      <ogo:labels>
        <ogo:label>
          <ogo:name>Reference designator</ogo:name>
          <ogo:text>@refdes</ogo:text>
          <ogo:position>(15 0)</ogo:position>
          <ogo:modify>yes</ogo:modify>
        </ogo:label>
        <ogo:label>
          <ogo:name>Resistance</ogo:name>
          <ogo:text>@res</ogo:text>
          <ogo:position>(15 30)</ogo:position>
          <ogo:modify>yes</ogo:modify>
        </ogo:label>
      </ogo:labels>
    </ogo:part>
```
This is a bit more compilicated than the symbol tag. First we define a name,
which is what is shown in the part list in the browser. The description is the
string shown below the part in the part preview. This could be a longer and more
descriptive string than the name, if needed. The symbol tag assigns the symbol
that should be used for this part (this way a symbol can be shared between 
several parts).

Then we can define properties. Most parts define Refdes, that is the reference
designator to use. For a resistor, we use R. Likewise, a capacitor would use C.
We also define a property named Res, which is the actual resistance. Finally,
we have Template, which is the template to use when generating spice netlists.
As you can see, when evaluating these strings, the proporties can be referred
to as `@<property name>`.

The use of label tags are more or less obvious. We can use properties, using the
@ character. For example, the label whose text is `<ogo:text>@refdes</ogo:text>`
will display the reference designator.

Part models
-----------

If you need to include a spice model for a part, you either add it inline in the
library or by including a model file.

For the former, you can do like this example (a diode):
```
    <ogo:name>Template</ogo:name>
    <ogo:value>D_@refdes %1 %2 M_@refdes \n.model M_@refdes (IS=0.1PA, RS=16 CJO=2PF TT=12N BV=100 IBV=0.1PA)</ogo:value>
```
This will add the model below each instance of the diode in the netlist.

For more complicated models, you should probably choose the latter method, which
is to add a property called Model. If the value of this property is, for instance,
PNP, the model file should be called PNP.model. The model file should be placed
in `<srcdir>/data/models/` and gets installed in `<prefix>/share/oregano/models`.

Note that we can NOT ship most models that can be found on the internet. Most of
these have some kind of restrictive license that keeps them from being used
commercially etc. So if anyone creates any models from scratch, I would be very
interested in adding them to Oregano. Perhaps we should start a library of free
part spice models.


Advanced macros
---------------

The reference macro `@<id>` is one of some macros defined in Oregano.
The full list of macros is described in this section.

**Rules:**
```
@<id>             value of <id>. If no value, error
&<id>             value of <id> if <id> is defined
?<id>s...s        text between s...s separators if <id> defined
?<id>s...ss...s   text between 1st s...s separators if <id> defined else 2nd s...s clause
~<id>s...s        text between s...s separators if <id> undefined
~<id>s...ss...s   text between 1st s...s separators if <id> undefined else 2nd s...s clause
#<id>s...s        text between s...s separators if <id> defined, but delete rest of template if <id> undefined
```
Separators can be any of {',', '.', ';', '/', '|', '(', ')'}.
For an opening-closing pair of separators the same character has to be used.

**Examples:**
```
R_@refdes %1 %2 @value
V_@refdes %1 %2 SIN(@offset @ampl @freq 0 0) ?DC|DC @DC|
```


Advanced parts
--------------

For parts that do more than just sit and wait for spice to handle them, there
might be a need to do some hacking. Examples of this are the parts Ground and
Jumper Wire. They all have a property called internal. The netlist generator looks
for this property and have special case code to handle them.

If you need to hack in some kind of special behaviour, take a look at netlist.c
and search for "internal", and "jumper". This should get you going.


Node referencing (17.05.2017)
-----------------------------

As you already noticed, you can reference local nodes by the '%' character.
With "local" I mean that only the nodes listed in the `<ogo:connections>`-tag can be referenced.

**definition: %\<number of connection\>**

Because the `<ogo:connection>` tag of oreglib files do not have a designator,
the only way to reference a connection is by its number. So the node
designator is implicitly defined by the occurrence-number in the oreglib file.

**example (oreglib)**

The following example is a "non-linear current source"
but because of the simple definition of the "Cur"-property,
it is a simple resistor with resistance of 1k
(as long as the "Cur"-property will not be changed by the user).
```
    <ogo:part>
      <ogo:name>nlcs</ogo:name>
      <ogo:symbol>resistor</ogo:symbol>
      <ogo:description>non-linear current source</ogo:description>
      <ogo:labels>
        <ogo:label>
          <ogo:name>Reference designator</ogo:name>
          <ogo:text>@refdes</ogo:text>
          <ogo:position>(15 0)</ogo:position>
          <ogo:modify>yes</ogo:modify>
        </ogo:label>
      </ogo:labels>
      <ogo:properties>
        <ogo:property>
          <ogo:name>Refdes</ogo:name>
          <ogo:value>G</ogo:value>
        </ogo:property>
        <ogo:property>
          <ogo:name>Cur</ogo:name>
          <ogo:value>'0.001*(V(%1)-V(%2))'</ogo:value>
        </ogo:property>
        <ogo:property>
          <ogo:name>Template</ogo:name>
          <ogo:value>@refdes %1 %2 cur=@cur</ogo:value>
        </ogo:property>
      </ogo:properties>
    </ogo:part>
```

**backward compatability**

Old oregano files will not work anymore because there have been used arbitrary
connection designators in the past (before 17.05.2017) very often. Arbitrary node
designators are not allowed any more.

To be backward compatible, it has been implemented an algorithm that detects old syntax
and the algorithm will transform the old syntax to the new syntax automatically.
The transformed circuit will not be the same under the following conditions:

- one reference designator is used more than once in `template`
AND the reference will actually be substitued more than once in `template`
AND the reference contains `%`
(this will lead to more referenced node connections than available)
OR
- there is a complex `template` expression
AND there are many `%`s
AND you are changing many expressions parts of many reference values
(these factors increase the probability of failure)
OR
- no further conditions known

so for your safety, if you open a file that
is older than 17.05.2017, you should save the file immediately after you opened it
to another location of your hard drive. If there is any unexpected error, please report
it at [this github website](https://github.com/drahnr/oregano/issues). The transformed
file will not be openable by older versions of Oregano (before 17.05.2017) likely.


The future
----------

So now when you learned how to create parts, I have some good news: the library
format is about to change ;) I'm moving to an SVG based format for the graphics,
and to a much saner approach for the rest of the format. There will not be support
for symbols. Instead each part will have to declare its own symbol. The benefit
of sharing symbols is not greater then the pain it causes. There are some other
nice changes planned as well, more about that later.
