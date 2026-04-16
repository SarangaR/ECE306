

Directions:

Homework #09
See course website for due date.


For this assignment, the task is to create three scrolling menus. The first menu will have 3 items; Resistors, Shapes and Song. Scrolling through these, a press of Button 1 should select the item and change to the chosen items set of options. In the case of Resistors, it should change to the Resistor codes, Shapes the shapes menu, and the Song should select the song. A press of Button 2 should step back to the main menu.

The thumb wheel is used to scroll through the menus. To create a menu, you will use the ADC value from the scroll wheel to identify a range for each item in the menu. The ADC is 12 bits, which yields a number from 0 [0x0000] to 4095 [0x0FFF]. For example, for a menu of 10 items, item 1 would be displayed for an ADC reading of 0 to 410, item 2 corresponds to 411-820, item 3 corresponds to 821-1230, etc. Depending on the number of items in the menu, the thumb wheel movement will be evenly divided.

There is an easier way to create a menu list based on a binary multiple. Assume you want 8 menu items, then if you right shift the 12-bit ADC value by 9 that will leave 3 bits remaining. 3 bits yields 0-7; 8 menu items.

The Resistor menu will display the Resistor Codes, as given in Table 1. As you scroll through the shapes, Line 1 and Line 3 will contain headers of Color and Value, Line 2 and Line 4 will show the Color and Value. The menu will end on the last color.

The Shapes menu will display the shapes, as given in Table 2. However, you need to change your display to be a 3 line display with the following function call [part of LCD.obj]:
lcd_BIG_mid();
To change back use the function: lcd_4line(void);
As you scroll through the shapes, the large line will show the current shape, the Line 1 is to show the previous shape, and Line 3 the next. Blank lines. Need to be used at either end to insure the shape desired is in the large line area.

The Song will select and Scroll through the Red and White song character by character on the big line. The Line 1 and Line 3 is to alternate back and forth scrolling Red and White / White and Red as you scroll through the song. Table 3 contains the song lyrics. Since this will be sensitive to scrolling if trying to accomplish this with one twist of the thumb wheel due to the large amount of characters, you will need to use several twists of the thumb wheel. Characters should only advance as the thumb wheel moves counter clock wise. Moving the thumbwheel clockwise should have no effect. Divide the ADC resolution into a "comfortable" scrolling range. Then split the characters up into these ranges, such that when you scroll one way through the ranges, it displays the first group of characters, then when scrolling back in the opposite direction has no effect, and then forward again advancing the song and so forth. This will allow you to scroll through the characters with a large enough Thumb Wheel space to read the words,

instead of having to slowly roll the Thumb Wheel in the hopes of catching every character in the song as you go. The words scroll on from the right and scroll off the left.

The code for each menu is to be in a separate function. The functions are to be in a new file called menus.c. Have your board display using the 3-line Big middle line, First Name on Line 1 and Last Name on Line 3 and "Homework 9" until a button is pressed. Which will start the menu items. Each function must have a descriptive header that describes author's name, date, what it is doing, what is passed in, what is returned, and any global that is modified. Your board may not be power cycled to go from menu to menu; it must be able to access all three menus at any time.

Do not forget the file header too.


Value
Color
0
Black
1
Brown
2
Red
3
Orange
4
Yellow
5
Green
6
Blue
7
Violet
8
Gray
9
White
Table 1 Green Menu - Resistor Codes


Circle
Square
Triangle
Octagon
Pentagon
Hexagon
Cube
Oval
Sphere
Cylinder
Table 2 Yellow Menu - Shapes


We're the Red and White from State And we know we are the best.
A hand behind our back, We can take on all the rest.

Come over the hill, Carolina. Devils and Deacs stand in line.
The Red and White from N.C. State.
Go State!

Table 3 Red Menu - Red and White Song


Demonstration Procedure:

Arrange a time through the sign-up mechanism to demonstrate your system to a TA. Make sure you have the TA sign your grade sheet.

The grading begins with your demonstration to a TA. This demonstration is external to using IAR. For item 1, does Name / Homework show until a button is pressed? 15 points. The TA will then check your menus to make sure they function and display properly. They will then check your files on your computer to check for #defines and proper headers and comments. You will lose 1 point for each numeric value used instead of a #define up to a maximum of 15 points. Do the files contain headers and are each function documented? 10 points.



Grading Scale for Homework:

Item
Description
Points
1
It works
15
2
Functions / file defined
10
3
Resistor Menu
10
4
Shape Menu
10
5
Red and White Song
25
6
Descriptive headers for functions
10
7
Use of #defines
10
8
Files Commented Properly
 10 


100


