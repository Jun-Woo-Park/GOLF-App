package com.jun.golf;

public class Object_Distance {

    public static float first_line_value;
    public static float second_line_value;
    public static float third_line_value;
    public static float fourth_line_value;
    public static float fifth_line_value;

    public void Distance(float first, float second, float third, float fourth, float fifth){
        first_line_value = first;
        second_line_value = second;
        third_line_value = third;
        fourth_line_value = fourth;
        fifth_line_value = fifth + 2;
    }
    public static String getDistanceLabel(Box box){
        String label_value = box.getLabel();
        return label_value;
    }

    public static float getDistance(Box box, Box[] results){
        float result = 0.F;
        float mal_pixel_value = 0.F;
        float average_value = 0.F;
        float pr_distance = 0.F;

        if (box.y1 >= first_line_value && box.y1 <= second_line_value) {
            mal_pixel_value = first_line_value - second_line_value;
            average_value = (float) (26.39 - 13.195);
            pr_distance = average_value/mal_pixel_value;

            result = (float) (13.195 +((box.y1 - second_line_value)*pr_distance));

        }else if (box.y1 > second_line_value && box.y1 <=third_line_value){
            mal_pixel_value = second_line_value - third_line_value;
            average_value = (float) (13.195 - 5.8);
            pr_distance = average_value/mal_pixel_value;

            result = (float) (5.8 +((box.y1 - third_line_value)*pr_distance));

        }else if (box.y1 > third_line_value && box.y1 <=fourth_line_value){
            mal_pixel_value = third_line_value - fourth_line_value;
            average_value = (float) (5.8 - 4.3);
            pr_distance = average_value/mal_pixel_value;

            result = (float) (4.3 +((box.y1 - fourth_line_value)*pr_distance));

        }else if (box.y1 > fourth_line_value && box.y1 <=fifth_line_value){
            mal_pixel_value = fourth_line_value - fifth_line_value;
            average_value = (float) (4.3 - 2.275);
            pr_distance = average_value/mal_pixel_value;

            result = (float) (2.275 +((box.y1 - fifth_line_value)*pr_distance));
        }

        return result;
    }//getDistance_Fn


}
